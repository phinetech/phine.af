```
af_core/
├── tests/
│   ├── integration/
│   │   ├── CMakeLists.txt
│   │   ├── README.md
│   │   ├── fixtures/                    # Test data and request fixtures
│   │   │   ├── qod/
│   │   │   │   ├── qod_create_session.json
│   │   │   │   ├── qod_update_session.json
│   │   │   │   └── qod_delete_session.json
│   │   │   └── device_location/
│   │   │       └── location_request.json
│   │   │
│   │   ├── framework/                   # Core test framework
│   │   │   ├── grpc_test_client.h       # Generic gRPC test client
│   │   │   ├── grpc_test_client.cpp
│   │   │   ├── test_fixture_loader.h    # Load JSON test fixtures
│   │   │   ├── test_fixture_loader.cpp
│   │   │   ├── test_assertions.h        # Custom assertions for gRPC responses
│   │   │   ├── test_assertions.cpp
│   │   │   ├── test_context.h           # Test environment setup
│   │   │   ├── test_context.cpp
│   │   │   └── response_validator.h     # Validate response schemas
│   │   │       └── response_validator.cpp
│   │   │
│   │   ├── suites/                      # Test suites organized by service
│   │   │   ├── qod_integration_test.cpp
│   │   │   ├── location_integration_test.cpp
│   │   │   └── end_to_end_test.cpp
│   │   │
│   │   ├── config/                      # Test configuration
│   │   │   ├── test_config.yaml
│   │   │   └── docker-compose.test.yaml
│   │   │
│   │   └── scripts/                     # Helper scripts
│   │       ├── run_tests.sh
│   │       ├── setup_test_env.sh
│   │       └── cleanup_test_env.sh
│   │
│   └── requests/                        # Your existing request files
│       └── qod/
│           └── qod_create_session.json
```

