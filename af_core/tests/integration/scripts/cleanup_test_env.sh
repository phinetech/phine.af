#!/bin/bash

set -e

echo "Cleaning up integration test environment..."

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$(dirname "$(dirname "$SCRIPT_DIR")")")"

# Stop docker-compose services
if [ -f "$PROJECT_ROOT/af_core/tests/integration/config/docker-compose.test.yaml" ]; then
    echo "Stopping test services..."
    docker-compose -f "$PROJECT_ROOT/af_core/tests/integration/config/docker-compose.test.yaml" down -v
fi

# Clean up test data
echo "Removing test data..."
# Add cleanup commands here

echo "Cleanup complete!"