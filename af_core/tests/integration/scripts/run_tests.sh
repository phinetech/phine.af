#!/bin/bash

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== AF Integration Test Runner ===${NC}"

# Configuration
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$SCRIPT_DIR")")")"
TEST_DIR="$PROJECT_ROOT/af_core/tests/integration"
BUILD_DIR="$PROJECT_ROOT/build"
RESULTS_DIR="$TEST_DIR/results"

# Parse arguments
RUN_SETUP=true
RUN_CLEANUP=true
TEST_FILTER="*"
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --no-setup)
            RUN_SETUP=false
            shift
            ;;
        --no-cleanup)
            RUN_CLEANUP=false
            shift
            ;;
        --filter)
            TEST_FILTER="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Create results directory
mkdir -p "$RESULTS_DIR"

# Setup test environment
if [ "$RUN_SETUP" = true ]; then
    echo -e "${YELLOW}Setting up test environment...${NC}"
    "$SCRIPT_DIR/setup_test_env.sh"
fi

# Wait for services to be ready
echo -e "${YELLOW}Waiting for gRPC service to be ready...${NC}"
MAX_RETRIES=30
RETRY_COUNT=0

while [ $RETRY_COUNT -lt $MAX_RETRIES ]; do
    if docker run --rm --network host fullstorydev/grpcurl -plaintext 192.168.70.141:50051 list > /dev/null 2>&1; then
        echo -e "${GREEN}Service is ready!${NC}"
        break
    fi
    
    RETRY_COUNT=$((RETRY_COUNT + 1))
    echo "Waiting for service... ($RETRY_COUNT/$MAX_RETRIES)"
    sleep 2
done

if [ $RETRY_COUNT -eq $MAX_RETRIES ]; then
    echo -e "${RED}Service failed to start in time${NC}"
    exit 1
fi

# Build tests if needed
if [ ! -f "$BUILD_DIR/af_core/tests/integration/af_integration_tests" ]; then
    echo -e "${YELLOW}Building integration tests...${NC}"
    cd "$BUILD_DIR"
    cmake --build . --target af_integration_tests
fi

# Run the tests
echo -e "${GREEN}Running integration tests...${NC}"
cd "$BUILD_DIR"

TEST_ARGS="--gtest_filter=$TEST_FILTER"
TEST_ARGS="$TEST_ARGS --gtest_output=xml:$RESULTS_DIR/test_results.xml"

if [ "$VERBOSE" = true ]; then
    TEST_ARGS="$TEST_ARGS --gtest_print_time=1"
fi

# Execute tests
if ./af_core/tests/integration/af_integration_tests $TEST_ARGS; then
    echo -e "${GREEN}✓ All tests passed!${NC}"
    TEST_EXIT_CODE=0
else
    echo -e "${RED}✗ Some tests failed${NC}"
    TEST_EXIT_CODE=1
fi

# Cleanup
if [ "$RUN_CLEANUP" = true ]; then
    echo -e "${YELLOW}Cleaning up test environment...${NC}"
    "$SCRIPT_DIR/cleanup_test_env.sh"
fi

# Generate summary
echo -e "${GREEN}=== Test Summary ===${NC}"
if [ -f "$RESULTS_DIR/test_results.xml" ]; then
    python3 "$SCRIPT_DIR/generate_report.py" "$RESULTS_DIR/test_results.xml"
fi

exit $TEST_EXIT_CODE