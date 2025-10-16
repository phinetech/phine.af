# AF Core Integration Tests

This directory contains the integration testing framework for the AF Core gRPC endpoints.

## Overview

The integration test framework provides:
- Automated gRPC endpoint testing
- Fixture-based test data management
- Custom assertions for gRPC responses
- Environment setup and teardown automation
- Support for both local and CI/CD execution

## Directory Structure

```
tests/
└── integration/
    ├── CMakeLists.txt
    ├── README.md
    ├── fixtures/                    # Test data and request fixtures
    │   ├── qod/
    │   │   ├── qod_create_session.json
    │   │   ├── qod_update_session.json
    │   │   └── qod_delete_session.json
    │   └── device_location/
    │       └── location_request.json
    │
    ├── framework/                   # Core test framework
    │   ├── grpc_test_client.h       # Generic gRPC test client
    │   ├── grpc_test_client.cpp
    │   ├── test_fixture_loader.h    # Load JSON test fixtures
    │   ├── test_fixture_loader.cpp
    │   ├── test_assertions.h        # Custom assertions for gRPC responses
    │   ├── test_assertions.cpp
    │   ├── test_context.h           # Test environment setup
    │   ├── test_context.cpp
    │   └── response_validator.h     # Validate response schemas
    │       └── response_validator.cpp
    │
    ├── suites/                      # Test suites organized by service
    │   ├── qod_integration_test.cpp
    │   ├── location_integration_test.cpp
    │   └── end_to_end_test.cpp
    │
    ├── config/                      # Test configuration
    │   ├── test_config.yaml
    │   └── docker-compose.test.yaml
    │
    └── scripts/                     # Helper scripts
        ├── run_tests.sh
        ├── setup_test_env.sh
        └── cleanup_test_env.sh
```

## Prerequisites

- CMake 3.15+
- C++17 compiler
- Google Test
- gRPC and Protocol Buffers
- Docker (for running test environment)

## Running Tests

### Quick Start

```bash
# Run all integration tests with environment setup
cd af_core/tests/integration
./scripts/run_tests.sh
```

### Run Specific Tests

```bash
# Run only QoD tests
./scripts/run_tests.sh --filter="QodIntegrationTest.*"

# Run without cleanup (useful for debugging)
./scripts/run_tests.sh --no-cleanup

# Run with verbose output
./scripts/run_tests.sh --verbose
```

### Manual Execution

```bash
# 1. Setup environment
./scripts/setup_test_env.sh

# 2. Build tests
cd ../../../build
cmake --build . --target af_integration_tests

# 3. Run tests
./af_core/tests/integration/af_integration_tests

# 4. Cleanup
cd ../af_core/tests/integration
./scripts/cleanup_test_env.sh
```

## Writing New Tests

### 1. Create a Test Suite

```cpp
#include "framework/test_context.h"
#include "framework/test_assertions.h"

namespace af::test {

TEST_F(QodIntegrationTest, MyNewTest) {
    // Arrange
    auto request = fixture_loader_->LoadFixture("qod/my_request.json");
    
    // Act
    auto response = client_->SendMessage(
        "message_type",
        request.dump(),
        GenerateCorrelationId()
    );
    
    // Assert
    ASSERT_GRPC_OK(response);
    ASSERT_FIELD_EQUALS(response, "status", "SUCCESS");
}

} // namespace af::test
```

### 2. Create Test Fixtures

Create JSON files in `fixtures/` directory:

```json
{
  "field1": "{{variable}}",
  "field2": "static_value"
}
```

Use template variables with `{{variable}}` syntax.

### 3. Load Fixtures in Tests

```cpp
// Load with variables
auto json = fixture_loader_->LoadFixtureWithVars(
    "qod/request.json",
    {{"variable", "value"}}
);

// Load as-is
auto json = fixture_loader_->LoadFixture("qod/request.json");
```

## Test Configuration

Edit `config/test_config.yaml` to configure:
- gRPC endpoint
- Timeout values
- Test data defaults
- Logging settings

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Integration Tests

on: [push, pull_request]

jobs:
  integration-tests:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      
      - name: Setup environment
        run: ./af_core/tests/integration/scripts/setup_test_env.sh
      
      - name: Build tests
        run: |
          mkdir build && cd build
          cmake .. -DBUILD_TESTS=ON
          cmake --build . --target af_integration_tests
      
      - name: Run tests
        run: ./af_core/tests/integration/scripts/run_tests.sh
      
      - name: Upload results
        uses: actions/upload-artifact@v2
        with:
          name: test-results
          path: af_core/tests/integration/results/
```

## Visual Flow

```
Compile Time:
qod_integration_test.cpp
    ↓
TEST_F macro expands
    ↓
Generates test class + static registration code
    ↓
Links into af_integration_tests executable

────────────────────────────────────────

Runtime (before main):
Static initializers run
    ↓
MakeAndRegisterTestInfo() called
    ↓
Tests added to GTest's global registry

────────────────────────────────────────

Runtime (in main):
main() calls InitGoogleTest()
    ↓
main() calls RUN_ALL_TESTS()
    ↓
GTest iterates registry and runs tests
    ↓
Results printed to console
```

## Troubleshooting

### Service not ready
If tests fail with "Service not ready", increase the timeout in `run_tests.sh`:
```bash
MAX_RETRIES=60  # Increase from 30
```

### Fixture not found
Ensure fixtures are copied to build directory. Check CMakeLists.txt:
```cmake
add_custom_command(TARGET af_integration_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        ${CMAKE_CURRENT_BINARY_DIR}/fixtures
)
```

### Connection refused
Check that the gRPC service is running and the endpoint is correct:
```bash
docker ps  # Verify containers are running
grpcurl -plaintext localhost:50051 list  # Test connection
```

## Best Practices

1. **Isolation**: Each test should be independent
2. **Cleanup**: Always cleanup created resources in teardown
3. **Meaningful names**: Use descriptive test names
4. **Assertions**: Use specific assertions from `test_assertions.h`
5. **Fixtures**: Reuse fixtures across tests
6. **Documentation**: Document complex test scenarios

## Support

For issues or questions:
- Check existing tests in `suites/` for examples
- Review framework code in `framework/`
- Consult the main project documentation