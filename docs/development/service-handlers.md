# Service Handlers & Transport Adapters

## Overview

The service handler architecture in the AF (Application Function) Core is designed to separate business logic from transport details using a composition-based approach. This structure allows service handlers to remain transport-agnostic while utilizing shared helpers to generate consistent responses for different protocols (gRPC, REST, etc.).

## Core Components

### 1. ServiceHandlerHelpers
**Location:** `common/handlers/service_handler_helpers.h`

This is the primary utility class for service handlers. It provides static methods to generate standardized responses without requiring handlers to inherit from a base class.

**Key Features:**
- **Standardized Error Handling:** creates consistent error responses with codes, messages, and correlation IDs.
- **Extension Points:** Supports lambda hooks (`build_extensions`) to inject service-specific fields into generic error payloads.
- **Logging Integration:** Optional integration with `spdlog` for debug tracing.

### 2. Transport Adapters
**Location:** `common/communication/adapters/`

Adapters bridge the gap between the internal domain `ResponseData` and the specific transport format (e.g., gRPC `Message`, HTTP Response).

- **GrpcResponseAdapter** (`grpc_response_adapter.h`): Converts internal response structures into the `af::communication::MessagePtr` format used by the gRPC communication layer. It handles:
    - Mapping HTTP status codes to gRPC status codes.
    - Serializing metadata (headers) and payloads.

- **RestResponseAdapter** (`rest_response_adapter.h`): <!-- TODO: detailed documentation on REST adapter usage -->

## Directory Structure

```text
common/
├── handlers/
│   ├── service_handler_helpers.h    # Shared helper utilities
│   └── request_context.h            # Context context tracking
├── communication/
│   └── adapters/
│       ├── grpc_response_adapter.h  # gRPC-specific translation
│       └── rest_response_adapter.h  # REST-specific translation
└── response/
    ├── response_builder.h           # Fluent builder for ResponseData
    └── response_data.h              # Transport-agnostic response model
```

## Usage Examples

### Creating an Error Response

Service handlers should use `ServiceHandlerHelpers::create_error_response` to return errors. This ensures all errors follow the same schema.

```cpp
#include "handlers/service_handler_helpers.h"

// In your handler method
af::communication::MessagePtr MyHandler::processRequest(const Request& req) {
    if (req.isInvalid()) {
        return af::common::handlers::ServiceHandlerHelpers::create_error_response(
            400,                        // HTTP Status
            "INVALID_REQUEST",          // Error Code
            "The request ID is missing",// Message
            req.correlation_id,         // Context
            m_logger                    // Logger
        );
    }
    // ...
}
```

### Extending Response Payloads

You can add custom fields to the error response using the extension lambda:

```cpp
auto response = ServiceHandlerHelpers::create_error_response(
    500, "INTERNAL_ERROR", "Processing failed", corr_id, logger,
    [](nlohmann::json& json_payload) {
        json_payload["retry_after"] = 30;
        json_payload["sub_system"] = "policy_engine";
    }
);
```

### Protocol Conversion

The conversion from internal data to the transport format often happens automatically inside the helpers using the adapters.

```cpp
// Internally, ServiceHandlerHelpers uses:
// auto grpc_msg = adapters::GrpcResponseAdapter::to_grpc_message(response_data);
```

## Future Improvements

- <!-- TODO: Add section on how to implement new Transport Adapters -->
- <!-- TODO: Document the RequestContext lifecycle -->
- <!-- TODO: Add sequence diagram reference for the request/response flow -->
