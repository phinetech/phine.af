#!/bin/bash
# Local CI/CD validation script
# Run this before pushing to validate your changes will pass CI/CD

set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}================================${NC}"
echo -e "${YELLOW}Local CI/CD Validation${NC}"
echo -e "${YELLOW}================================${NC}"
echo ""

# Function to print status
print_status() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓${NC} $2"
    else
        echo -e "${RED}✗${NC} $2"
        return 1
    fi
}

# Change to repository root
cd "$(git rev-parse --show-toplevel)"

echo -e "${YELLOW}Step 1: Code Quality Checks${NC}"
echo "----------------------------"

# Check for gtp5g kernel module
echo "Checking for gtp5g kernel module..."
if ! lsmod | grep -q gtp5g; then
    echo -e "${YELLOW}gtp5g module not found. Installing...${NC}"
    echo "This requires sudo privileges and will install kernel module dependencies."

    # Check if running with sudo
    if [ "$EUID" -ne 0 ]; then
        echo -e "${RED}Please run this script with sudo to install gtp5g module${NC}"
        echo "Usage: sudo $0"
        exit 1
    fi

    apt-get update
    apt-get install -y git build-essential linux-headers-$(uname -r)

    cd /tmp
    git clone --depth 1 https://github.com/free5gc/gtp5g.git
    cd gtp5g/
    make
    make install

    echo "Loading gtp5g module..."
    modprobe gtp5g

    # Return to original directory
    cd "$(git rev-parse --show-toplevel)"

    if lsmod | grep -q gtp5g; then
        print_status 0 "gtp5g module installed and loaded successfully"
    else
        print_status 1 "Failed to load gtp5g module"
        exit 1
    fi
else
    print_status 0 "gtp5g module already loaded"
fi

# Check for trailing whitespace
echo -n "Checking for trailing whitespace... "
if git diff --check; then
    print_status 0 "No trailing whitespace found"
else
    print_status 1 "Trailing whitespace found (run: git diff --check)"
fi

# Check shell script permissions
echo -n "Checking shell script permissions... "
missing_exec=$(find . -type f -name "*.sh" -not -executable | wc -l)
if [ "$missing_exec" -eq 0 ]; then
    print_status 0 "All shell scripts are executable"
else
    print_status 1 "$missing_exec shell scripts are not executable"
    find . -type f -name "*.sh" -not -executable
fi

echo ""
echo -e "${YELLOW}Step 2: Build Components${NC}"
echo "----------------------------"

# Build AF Core
echo "Building AF Core..."
if docker build -f af_core/Dockerfile -t af-core:validation af_core/ > /tmp/af-core-build.log 2>&1; then
    print_status 0 "AF Core built successfully"
else
    print_status 1 "AF Core build failed (see /tmp/af-core-build.log)"
    tail -20 /tmp/af-core-build.log
    exit 1
fi

# Build PCF Handler
echo "Building PCF Handler..."
if docker build -f southbound/pcf_handler/Dockerfile -t pcf-handler:validation . > /tmp/pcf-handler-build.log 2>&1; then
    print_status 0 "PCF Handler built successfully"
else
    print_status 1 "PCF Handler build failed (see /tmp/pcf-handler-build.log)"
    tail -20 /tmp/pcf-handler-build.log
    exit 1
fi

# Check PCF Handler dependencies
echo -n "Checking PCF Handler dependencies... "
if docker run --rm pcf-handler:validation sh -c "ldd /usr/local/bin/pcf_server | grep 'not found'" > /dev/null 2>&1; then
    print_status 1 "Missing dependencies detected"
    docker run --rm pcf-handler:validation sh -c "ldd /usr/local/bin/pcf_server | grep 'not found'"
else
    print_status 0 "All dependencies resolved"
fi

# Build API Component
echo "Building API Component..."
if docker build -f northbound/api_component/Dockerfile -t api-component:validation northbound/api_component/ > /tmp/api-component-build.log 2>&1; then
    print_status 0 "API Component built successfully"
else
    print_status 1 "API Component build failed (see /tmp/api-component-build.log)"
    tail -20 /tmp/api-component-build.log
    exit 1
fi

echo ""
echo -e "${YELLOW}Step 3: Build Integration Tests${NC}"
echo "----------------------------"

echo "Building integration tests..."
if docker build -f af_core/tests/integration/Dockerfile -t integration-tests:validation af_core/tests/integration/ > /tmp/integration-tests-build.log 2>&1; then
    print_status 0 "Integration tests built successfully"
else
    print_status 1 "Integration tests build failed (see /tmp/integration-tests-build.log)"
    tail -20 /tmp/integration-tests-build.log
    exit 1
fi

echo ""
echo -e "${YELLOW}Step 4: Run Integration Tests${NC}"
echo "----------------------------"
echo "Starting integration tests (this will take ~10-15 minutes)..."
echo "Test logs will be saved to /tmp/integration-tests.log"

cd docker-compose

# Ensure cleanup happens on exit
cleanup_integration_tests() {
    echo "Cleaning up integration test environment..."
    docker-compose -f docker-compose-test.yaml down -v
    cd ..
}
trap cleanup_integration_tests EXIT

# Start services in background
echo "Starting services..."
if ! docker-compose -f docker-compose-test.yaml up --build -d; then
    print_status 1 "Failed to start docker-compose services"
    exit 1
fi

# Wait a moment for container to be created
sleep 2

# Get the actual container name
CONTAINER_NAME=$(docker-compose -f docker-compose-test.yaml ps -q af_integration_tests 2>/dev/null)

if [ -z "$CONTAINER_NAME" ]; then
    print_status 1 "Integration tests container not found"
    echo "Available containers:"
    docker-compose -f docker-compose-test.yaml ps
    exit 1
fi

echo "Following test logs from container: $CONTAINER_NAME"

# Follow logs from af_integration_tests container only
docker logs -f "$CONTAINER_NAME" > /tmp/integration-tests.log 2>&1 &
LOG_PID=$!

# Wait for the test container to finish
if docker wait "$CONTAINER_NAME" > /dev/null 2>&1; then
    # Stop background log following
    kill $LOG_PID 2>/dev/null || true
    wait $LOG_PID 2>/dev/null || true

    # Get exit code
    EXIT_CODE=$(docker inspect "$CONTAINER_NAME" --format='{{.State.ExitCode}}')

    # Collect docker logs for all containers
    echo "Collecting logs from all containers..."
    mkdir -p /tmp/container-logs
    for container in $(docker-compose -f docker-compose-test.yaml ps -q); do
        cname=$(docker inspect --format='{{.Name}}' "$container" | sed 's/^\/\(.*\)/\1/')
        docker logs "$container" > "/tmp/container-logs/${cname}.log" 2>&1
    done

    if [ "$EXIT_CODE" -eq 0 ]; then
        print_status 0 "Integration tests passed"
        echo "Full logs available at: /tmp/integration-tests.log"
    else
        print_status 1 "Integration tests failed (exit code: $EXIT_CODE)"
        echo ""
        echo -e "${RED}Last 50 lines of integration test output:${NC}"
        tail -50 /tmp/integration-tests.log
        echo ""
        echo "Full logs: /tmp/integration-tests.log"
        exit 1
    fi
else
    kill $LOG_PID 2>/dev/null || true
    echo "Full test logs: /tmp/integration-tests.log"

    print_status 1 "Failed to wait for integration tests container"
    exit 1
fi

echo ""
echo -e "${YELLOW}Step 5: Check Docker Image Sizes${NC}"
echo "----------------------------"
docker images | grep validation | awk '{print $1 "\t" $7 " " $8}'

echo ""
echo -e "${GREEN}================================${NC}"
echo -e "${GREEN}✓ All validation checks passed!${NC}"
echo -e "${GREEN}================================${NC}"
echo ""
echo "Your changes are ready to be pushed."
echo "CI/CD workflows will run automatically on PR creation/update."
echo ""
echo "Useful commands:"
echo "  - View build logs: tail /tmp/*-build.log"
echo "  - View integration test logs: tail /tmp/integration-tests.log"
echo "  - Clean up: docker rmi af-core:validation pcf-handler:validation api-component:validation integration-tests:validation"
echo "  - Run specific tests: cd docker-compose && docker-compose -f docker-compose-test.yaml run --rm -e GTEST_FILTER='TestName.*' af_integration_tests"
