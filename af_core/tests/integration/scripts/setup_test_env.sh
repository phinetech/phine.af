#!/bin/bash

set -e

echo "Setting up integration test environment..."

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$SCRIPT_DIR")")")"

# Load test configuration
CONFIG_FILE="$PROJECT_ROOT/af_core/tests/integration/config/test_config.yaml"

# Start necessary services using docker-compose
if [ -f "$PROJECT_ROOT/af_core/tests/integration/config/docker-compose.test.yaml" ]; then
    echo "Starting test services..."
    docker-compose -f "$PROJECT_ROOT/af_core/tests/integration/config/docker-compose.test.yaml" up -d
fi

# Initialize test database/state if needed
echo "Initializing test data..."

# Create test UE entries
# Add any other test data initialization here

echo "Test environment setup complete!"