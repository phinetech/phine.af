#!/bin/bash
set -e

# ci_helper.sh
# Helper script for CI/CD workflow tasks
# Usage: ./ci_helper.sh [command] [arguments...]

function install_dependencies() {
    echo "Checking for required dependencies..."
    if ! command -v curl &> /dev/null; then
        echo "curl not found, installing..."
        sudo apt-get update && sudo apt-get install -y curl
        echo "curl installed successfully."
    else
        echo "curl is already installed."
    fi

    if ! command -v grpcurl &> /dev/null; then
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
            if curl -s "http://${NRF_IP}:8000/nnrf-nfm/v1/nf-instances" > /dev/null 2>&1; then
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
    echo "Waiting for AF Core to be ready..."

    for i in $(seq 1 $retries); do
        # Get AF Core container IP
        AF_IP=$(docker inspect -f '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' af-core 2>/dev/null || true)

        if [ -n "$AF_IP" ]; then
            if grpcurl -plaintext "${AF_IP}:50051" list > /dev/null 2>&1; then
                echo "AF Core is ready!"
                return 0
            fi
        fi
        echo "Waiting for AF Core... ($i/$retries)"
        sleep $wait_time
    done

    echo "Timeout waiting for AF Core"
    return 1
}

function collect_logs() {
    local LOG_DIR=${1:-"docker-compose/logs"}
    mkdir -p "$LOG_DIR"
    echo "Collecting container logs to: $LOG_DIR"

    # Collect overall container status
    echo "=== Status of all containers ===" > "$LOG_DIR/container_status.log"
    docker ps -a >> "$LOG_DIR/container_status.log" 2>&1 || echo "Failed to get container status"

    # List of containers
    local CONTAINERS=(
        "af-core:af_core.log"
        "af-north-api:api_component.log"
        "af-pcf-handler:pcf_handler.log"
        "af-integration-tests:integration_tests.log"
        "demo-qod-adapter:demo_qod_adapter.log"
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
        echo "=== $container_name Logs ===" > "$LOG_DIR/$log_file"

        if docker ps -a --format '{{.Names}}' | grep -q "^${container_name}$"; then
            docker logs "$container_name" >> "$LOG_DIR/$log_file" 2>&1 || echo "Failed to get $container_name logs"
        else
            echo "Container $container_name not found" >> "$LOG_DIR/$log_file"
        fi
    done
}

function parse_results() {
    local RESULTS_FILE=${1:-"docker-compose/results/test_results.xml"}

    if [ -f "$RESULTS_FILE" ]; then
        echo "Test results found!"
        # Display summary
        if command -v xmllint > /dev/null; then
            xmllint --format "$RESULTS_FILE" | grep -E "(tests=|failures=|errors=)" || true
        fi
        cat "$RESULTS_FILE"
    else
        echo "No test results file found at $RESULTS_FILE"
        exit 0 # Don't fail the build just because xml is missing, usually strictly handled by exit code
    fi
}

function cleanup_services() {
    echo "Cleaning up services..."
    if [ -d "docker-compose" ]; then
        cd docker-compose
        docker compose -f docker-compose-test.yaml down -v
        cd ..
        echo "Services removed."
    else
        echo "Directory 'docker-compose' not found."
    fi
}

function build_image() {
    local service_name=$1
    local tag=${2:-latest}

    if [ -z "$service_name" ]; then
        echo "Usage: $0 build <service_name> [tag]"
        exit 1
    fi

    # Ensure submodules are initialized and up to date before building.
    check_submodules

    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
    local compose_dir="${PROJECT_ROOT}/docker-compose"

    if [ ! -d "$compose_dir" ]; then
        echo "Directory 'docker-compose' not found."
        exit 1
    fi

    echo "Building ${service_name} with tag ${tag}..."
    (cd "$compose_dir" && TAG="$tag" docker compose -f docker-compose-test.yaml build "$service_name")
}

function build_all_images() {
    local tag=${1:-latest}

    # Ensure submodules are initialized and up to date before building.
    check_submodules

    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
    local compose_dir="${PROJECT_ROOT}/docker-compose"

    if [ ! -d "$compose_dir" ]; then
        echo "Directory 'docker-compose' not found."
        exit 1
    fi

    local services=(af_core pcf_handler api_component af-integration-tests)

    echo "Building all AF images with tag ${tag}..."
    (cd "$compose_dir" && TAG="$tag" docker compose -f docker-compose-test.yaml build "${services[@]}")
}

function run_adapter_test() {
    echo "Running adapter integration test with full 5G core..."

    # 0. Check and initialize submodules
    check_submodules

    # 1. Install dependencies
    install_dependencies

    # 2. Install gtp5g
    install_gtp5g

    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
    local compose_dir="${PROJECT_ROOT}/docker-compose"

    if [ ! -d "$compose_dir" ]; then
        echo "Directory 'docker-compose' not found."
        exit 1
    fi

    echo "Ensuring docker-compose/logs directory exists..."
    mkdir -p "${compose_dir}/logs"
    mkdir -p "${compose_dir}/results"
    chmod 777 "${compose_dir}/results"

    echo "Building and starting services via docker-compose..."
    cd "$compose_dir"

    # Pull base images
    docker compose -f docker-compose-test.yaml pull db free5gc-nrf free5gc-amf free5gc-upf oai-ext-dn

    # Build AF components + adapter (skip api_component and af-integration-tests)
    docker compose -f docker-compose-test.yaml build af_core pcf_handler demo-qod-adapter

    # Start infrastructure
    docker compose -f docker-compose-test.yaml up -d db free5gc-nrf free5gc-amf free5gc-ausf free5gc-nssf free5gc-pcf free5gc-smf free5gc-udm free5gc-udr free5gc-upf free5gc-webui oai-ext-dn

    # Wait for NRF
    cd ..
    wait_for_nrf
    cd "$compose_dir"

    # Start UE and gNB
    docker compose -f docker-compose-test.yaml up -d gnb ue
    echo "Waiting for UE to establish connection (20s)..."
    sleep 20

    # Start AF core + southbound handler (skip api_component)
    docker compose -f docker-compose-test.yaml up -d af_core pcf_handler

    # Wait for AF
    cd ..
    wait_for_af
    cd "$compose_dir"

    echo "Running demo-qod-adapter..."
    set +e
    docker compose -f docker-compose-test.yaml up --exit-code-from demo-qod-adapter demo-qod-adapter
    local EXIT_CODE=$?
    set -e

    cd ..

    # Collect logs regardless of success/failure
    collect_logs "docker-compose/logs"

    if [ $EXIT_CODE -eq 0 ]; then
        echo "SUCCESS: Adapter integration test passed!"
    else
        echo "FAILURE: Adapter integration test failed with exit code $EXIT_CODE"
        exit $EXIT_CODE
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

function run_all_local_validation() {
    echo "Running full local validation sequence..."

    # 0. Check and initialize submodules
    check_submodules

    # 1. Install dependencies
    install_dependencies

    # 2. Install gtp5g
    install_gtp5g

    echo "Ensuring docker-compose/logs directory exists..."
    mkdir -p docker-compose/logs
    mkdir -p docker-compose/results
    chmod 777 docker-compose/results

    echo "Building and starting services via docker-compose..."
    cd docker-compose

    # Pull base images
    docker compose -f docker-compose-test.yaml pull db free5gc-nrf free5gc-amf free5gc-upf oai-ext-dn

    # Build
    docker compose -f docker-compose-test.yaml build af_core pcf_handler api_component af-integration-tests

    # Start infrastructure
    docker compose -f docker-compose-test.yaml up -d db free5gc-nrf free5gc-amf free5gc-ausf free5gc-nssf free5gc-pcf free5gc-smf free5gc-udm free5gc-udr free5gc-upf free5gc-webui oai-ext-dn

    # Wait for NRF
    cd ..
    wait_for_nrf
    cd docker-compose

    # Start UE and gNB
    docker compose -f docker-compose-test.yaml up -d gnb ue
    echo "Waiting for UE to establish connection (20s)..."
    sleep 20

    # Start AF components
    docker compose -f docker-compose-test.yaml up -d af_core pcf_handler api_component

    # Wait for AF
    cd ..
    wait_for_af
    cd docker-compose

    echo "Running integration tests..."
    set +e # Don't exit immediately on test failure, we want logs
    docker compose -f docker-compose-test.yaml up --exit-code-from af-integration-tests af-integration-tests
    TEST_EXIT_CODE=$?
    set -e

    cd ..

    # Collect logs regardless of success/failure
    collect_logs "docker-compose/logs"

    if [ $TEST_EXIT_CODE -eq 0 ]; then
        echo "SUCCESS: Integration tests passed!"
        parse_results "docker-compose/results/test_results.xml"
    else
        echo "FAILURE: Integration tests failed with exit code $TEST_EXIT_CODE"
        parse_results "docker-compose/results/test_results.xml"
        exit $TEST_EXIT_CODE
    fi
}

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
    cleanup_services)
        cleanup_services
        ;;
    build)
        build_image "$@"
        ;;
    build_all)
        build_all_images "$@"
        ;;
    build_standalone)
        build_standalone "$@"
        ;;
    run_adapter_test)
        run_adapter_test
        ;;
    parse_results)
        parse_results "$@"
        ;;
    "")
        # Default behavior if no command provided
        run_all_local_validation
        ;;
    *)
        echo "Usage: $0 {check_submodules|install_dependencies|install_gtp5g|wait_for_nrf|wait_for_af|collect_logs|parse_results|cleanup_services|build|build_all|build_standalone|run_adapter_test}"
        exit 1
        ;;
esac
