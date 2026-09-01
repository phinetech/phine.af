#!/bin/bash
set -e

# ci_helper.sh
# Helper script for CI/CD workflow tasks
# Usage: ./ci_helper.sh [command] [arguments...]

function install_dependencies() {
    echo "Checking for required dependencies..."
    if ! command -v curl &>/dev/null; then
        echo "curl not found, installing..."
        sudo apt-get update && sudo apt-get install -y curl
        echo "curl installed successfully."
    else
        echo "curl is already installed."
    fi

    if ! command -v grpcurl &>/dev/null; then
        echo "grpcurl not found, installing..."
        sudo snap install --edge grpcurl
        echo "grpcurl installed successfully."
    else
        echo "grpcurl is already installed."
    fi
}

function install_gtp5g() {
    echo "Checking for gtp5g kernel module..."
    if ! lsmod | grep -q gtp5g; then
        echo "gtp5g module not found, installing..."
        sudo apt-get update
        sudo apt-get install -y git build-essential linux-headers-$(uname -r)

        # Create temp dir for building
        TEMP_DIR=$(mktemp -d)
        cd "$TEMP_DIR"

        git clone --depth 1 https://github.com/free5gc/gtp5g.git
        cd gtp5g/
        make
        sudo make install

        echo "Loading gtp5g module..."
        sudo modprobe gtp5g

        # Cleanup
        cd /
        rm -rf "$TEMP_DIR"

        lsmod | grep gtp5g
        echo "gtp5g module installed and loaded successfully!"
    else
        echo "gtp5g module already loaded"
        lsmod | grep gtp5g
    fi
}

function check_submodules() {
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

    if [ -f "${PROJECT_ROOT}/.gitmodules" ]; then
        echo "Checking submodules..."
        cd "${PROJECT_ROOT}"

        # Check if any submodules are uninitialized (start with -)
        if git submodule status | grep -q '^-'; then
            echo "⚠️  Uninitialized submodules detected!"
            echo "Initializing submodules automatically..."
            git submodule update --init --recursive
            echo "✓ Submodules initialized successfully"
        else
            # Check if submodules are out of sync
            if git submodule status | grep -q '^+'; then
                echo "⚠️  Submodules are out of sync with the current commit"
                echo "Updating submodules..."
                git submodule update --recursive
                echo "✓ Submodules updated successfully"
            else
                echo "✓ All submodules are up to date"
            fi
        fi
    fi
}

function wait_for_nrf() {
    local retries=30
    local wait_time=2
    echo "Waiting for Free5GC NRF to be ready..."

    for i in $(seq 1 $retries); do
        # Get NRF container IP
        NRF_IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' nrf 2>/dev/null || true)

        if [ -n "$NRF_IP" ]; then
            if curl -s "http://${NRF_IP}:8000/nnrf-nfm/v1/nf-instances" >/dev/null 2>&1; then
                echo "NRF is ready!"
                return 0
            fi
        fi
        echo "Waiting for NRF... ($i/$retries)"
        sleep $wait_time
    done

    echo "Timeout waiting for NRF"
    return 1
}

function wait_for_af() {
    local retries=30
    local wait_time=2
    echo "Waiting for AF gRPC endpoint to be ready..."

    for i in $(seq 1 $retries); do
        local AF_CONTAINER=""

        if docker ps -a --format '{{.Names}}' | grep -q '^phine.af-core$'; then
            AF_CONTAINER="phine.af-core"
        elif docker ps -a --format '{{.Names}}' | grep -q '^af$'; then
            AF_CONTAINER="af"
        fi

        if [ -n "$AF_CONTAINER" ]; then
            AF_IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' "$AF_CONTAINER" 2>/dev/null || true)
        else
            AF_IP=""
        fi

        if [ -n "$AF_IP" ]; then
            if grpcurl -plaintext "${AF_IP}:50051" list >/dev/null 2>&1; then
                echo "AF gRPC endpoint is ready (${AF_CONTAINER})!"
                return 0
            fi
        fi
        echo "Waiting for AF endpoint... ($i/$retries)"
        sleep $wait_time
    done

    echo "Timeout waiting for AF endpoint"
    return 1
}

function collect_logs() {
    local LOG_DIR=${1:-"docker-compose/logs"}
    mkdir -p "$LOG_DIR"
    echo "Collecting container logs to: $LOG_DIR"

    # Collect overall container status
    echo "=== Status of all containers ===" >"$LOG_DIR/container_status.log"
    docker ps -a >>"$LOG_DIR/container_status.log" 2>&1 || echo "Failed to get container status"

    # List of containers
    local CONTAINERS=(
        "af:af.log"
        "phine.af-core:af_core.log"
        "phine.af-api:api_component.log"
        "phine.af-pcf-handler:pcf_handler.log"
        "phine.af-demo-qod:af_demo_qod.log"
        "phine.af-demo-qod-adapter:demo_qod_adapter.log"
        "af-integration-tests:integration_tests.log"
        "pcf:pcf.log"
        "ue:ue.log"
        "gnb:gnb.log"
        "mongodb:mongodb.log"
        "nrf:nrf.log"
        "amf:amf.log"
        "smf:smf.log"
        "udr:udr.log"
        "udm:udm.log"
        "ausf:ausf.log"
        "upf:upf.log"
        "oai-ext-dn:oai-ext-dn.log"
    )

    for entry in "${CONTAINERS[@]}"; do
        local container_name="${entry%%:*}"
        local log_file="${entry##*:}"

        echo "Collecting logs for $container_name..."
        echo "=== $container_name Logs ===" >"$LOG_DIR/$log_file"

        if docker ps -a --format '{{.Names}}' | grep -q "^${container_name}$"; then
            docker logs "$container_name" >>"$LOG_DIR/$log_file" 2>&1 || echo "Failed to get $container_name logs"
        else
            echo "Container $container_name not found" >>"$LOG_DIR/$log_file"
        fi
    done
}

function parse_results() {
    local RESULTS_FILE=${1:-"docker-compose/results/test_results.xml"}

    if [ -f "$RESULTS_FILE" ]; then
        echo "Test results found!"
        # Display summary
        if command -v xmllint >/dev/null; then
            xmllint --format "$RESULTS_FILE" | grep -E "(tests=|failures=|errors=)" || true
        fi
        cat "$RESULTS_FILE"
    else
        echo "No test results file found at $RESULTS_FILE"
        exit 0 # Don't fail the build just because xml is missing, usually strictly handled by exit code
    fi
}

function build_standalone() {
    # Ensure submodules are initialized and up to date before building.
    check_submodules

    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
    local BUILD_DIR="${PROJECT_ROOT}/build-output"

    local CLEAN=0
    local BUILD_TYPE="Release"
    local BUILD_TESTS=0
    local INSTALL=0
    local JOBS
    JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

    while [[ $# -gt 0 ]]; do
        case $1 in
        --clean)
            CLEAN=1
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --tests)
            BUILD_TESTS=1
            shift
            ;;
        --install)
            INSTALL=1
            shift
            ;;
        --help)
            echo "Usage: $0 build_standalone [options]"
            echo "Options:"
            echo "  --clean        Clean build directory before building"
            echo "  --debug        Build in debug mode"
            echo "  --tests        Build tests"
            echo "  --install      Install after building"
            echo "  --help         Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
        esac
    done

    echo "Running standalone root CMake build..."
    echo "  Project root: ${PROJECT_ROOT}"
    echo "  Build directory: ${BUILD_DIR}"
    echo "  Build type: ${BUILD_TYPE}"
    echo "  Clean build: ${CLEAN}"
    echo "  Build tests: ${BUILD_TESTS}"
    echo "  Install after build: ${INSTALL}"
    echo "  Parallel jobs: ${JOBS}"

    if [ $CLEAN -eq 1 ] && [ -d "${BUILD_DIR}" ]; then
        echo "Cleaning build directory..."
        rm -rf "${BUILD_DIR}"
    fi

    if [ ! -d "${BUILD_DIR}" ]; then
        echo "Creating build directory..."
        mkdir -p "${BUILD_DIR}"
    fi

    cd "${BUILD_DIR}"

    echo "Configuring CMake..."
    cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DBUILD_TESTS=$([[ $BUILD_TESTS -eq 1 ]] && echo "ON" || echo "OFF") \
        "${PROJECT_ROOT}"

    echo "Building..."
    cmake --build . -- -j${JOBS}

    if [ $INSTALL -eq 1 ]; then
        echo "Installing..."
        cmake --install .
    fi
}

# Main dispatch
COMMAND=$1
shift || true

case "$COMMAND" in
install_dependencies)
    install_dependencies
    ;;
install_gtp5g)
    install_gtp5g
    ;;
check_submodules)
    check_submodules
    ;;
wait_for_nrf)
    wait_for_nrf
    ;;
wait_for_af)
    wait_for_af
    ;;
collect_logs)
    collect_logs "$@"
    ;;
build_standalone)
    build_standalone "$@"
    ;;
parse_results)
    parse_results "$@"
    ;;
*)
    echo "Usage: $0 {check_submodules|install_dependencies|install_gtp5g|wait_for_nrf|wait_for_af|collect_logs|parse_results|build_standalone}"
    exit 1
    ;;
esac
