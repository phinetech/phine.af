# HTTP/2 Transport Architecture

This document describes the HTTP/2 communication transport layer built on **nghttp2** and **Boost.Asio**. It explains the library choices, component design, message contract, and how the transport is used across the three communication links in the system.

---

## Overview

The HTTP/2 transport is a pluggable implementation of the `CommunicationService` interface. It can be selected at runtime via configuration (`kind: http`) and is managed through the same Dependency Injection (DI) pipeline as gRPC and Direct transports.

It serves three distinct communication roles:

| Link | Direction | Purpose |
|------|-----------|---------|
| Adapter → AF Core | Northbound inbound | External adapter sends CAMARA requests to AF Core |
| AF Core ↔ PCF Handler | Southbound internal (bidirectional) | AF Core sends requests; PCF Handler sends notifications back |
| PCF Handler → 5G Core (PCF) | Southbound external | PCF Handler calls the real 5G PCF via Npcf_PolicyAuthorization API |

All three use the same underlying transport library but differ in message contract and routing semantics.

---

## Libraries

| Library | Role | Why |
|---------|------|-----|
| **nghttp2** | HTTP/2 frame layer | Low-level HTTP/2 session management (client and server). Already used by the PCF Handler southbound path. |
| **Boost.Asio** | Networking (TCP sockets, async I/O) | Portable, async-capable, already a project dependency via Boost. |
| **OpenSSL** | TLS/ALPN (optional) | Required for `https://` targets. Optional for cleartext `h2c`. |
| **nlohmann/json** | JSON serialization | Serializes the internal `Message` envelope. Already used throughout the project. |

---

## Component Architecture

The implementation lives in `common/communication/implementations/http/` and consists of three layers:

```
┌─────────────────────────────────────────────────────────────────┐
│                  HttpCommunicationService                        │
│         (implements CommunicationService interface)              │
│                                                                 │
│  ┌───────────────────────┐    ┌────────────────────────────┐   │
│  │  Http2ClientTransport │    │   Http2ServerTransport     │   │
│  │  (outbound requests)  │    │   (inbound request accept) │   │
│  └───────────────────────┘    └────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
                          │                        │
                     nghttp2 + Boost.Asio     nghttp2 + Boost.Asio
                          │                        │
                      TCP / TLS                TCP / TLS
```

### Http2ClientTransport

A reusable, domain-free HTTP/2 client. Responsibilities:

- Parse target URL into host/port/path
- Manage a single TCP connection with HTTP/2 multiplexing
- Submit requests with arbitrary method, path, headers, and body
- Accumulate per-stream responses via nghttp2 callbacks
- Handle connection lifecycle (connect, reconnect, disconnect)
- Configurable timeouts, concurrency limits, retry policy

### Http2ServerTransport

A reusable HTTP/2 server. Responsibilities:

- Bind to a local address/port and accept TCP connections
- Run nghttp2 server sessions per connection
- Route inbound requests to registered path handlers
- Submit HTTP/2 responses back to clients
- Support concurrent connections and streams

### HttpCommunicationService

The top-level class that implements `af::communication::CommunicationService`. It composes the client and server transports and bridges them to the AF message model.

- **Client-only mode** (`client_only: true`): Only the client transport is instantiated. Used when the component only sends outbound requests.
- **Server+Client mode** (default): Both transports are active. The server receives inbound messages and the client sends outbound requests.

---

## Internal Message Contract

For communication **between AF components** (AF Core ↔ PCF Handler), the HTTP/2 transport uses the same `InternalMessage` structure used by gRPC. The wire format is JSON over HTTP/2 instead of Protobuf over gRPC.

### Message-Type Routing (Internal Communication)

For internal AF-to-AF communication, messages are routed by the `message_type` field in the payload.

#### Endpoint

```
### Message Envelope (gRPC/Direct Transports Only)

Internal AF components communicate using a message envelope. This is supported on **gRPC and Direct transports only**, not HTTP.
Content-Type: application/json
```

> **Note:** The `/internal/messages` endpoint is **removed** for northbound HTTP communication. External clients should use the CAMARA REST API endpoints instead (see REST API Routing section below).

#### Request Body

```json
{
  "message_type": "pcf_create_qod_session",
  "correlation_id": "550e8400-e29b-41d4-a716-446655440000",
  "payload": { ... },
  "metadata": {
    "source": "af_core",
    "priority": "normal"
  }
}
```

### Response Body

```json
{
  "message_type": "pcf_create_qod_session_response",
  "correlation_id": "550e8400-e29b-41d4-a716-446655440000",
  "payload": { ... },
  "metadata": {
    "status_code": "200"
  }
}
```

### Field Mapping

| `Message` struct field | JSON field | Description |
|------------------------|-----------|-------------|
| `message_type` | `message_type` | Identifies the operation (e.g., `qod_create_session`) |
| `correlation_id` | `correlation_id` | Links request to response |
| `payload` | `payload` | Business data (JSON object or raw string) |
| `metadata` | `metadata` | Key-value pairs for routing, status, tracing |

This means **all existing handler registrations remain unchanged**. A handler registered with:

```cpp
comm->register_handler("pcf_create_qod_session", my_handler);
```

works identically whether the transport is gRPC, Direct, or HTTP.

---

## REST API Routing (External Communication)

For **northbound HTTP communication** (external clients → AF Core), the HTTP transport now supports **CAMARA REST API endpoints** instead of the internal message envelope.

### Architecture

The REST API routing is implemented through a two-tier system:

1. **Transport Layer** (common library): Routes by HTTP method + path
2. **Application Layer** (af_core): Maps REST endpoints to internal message handlers

```
External Client (CAMARA)
     │
     ├─ HTTP: POST /quality-on-demand/v1/sessions
     │         │
     │         ▼
     │    HttpCommunicationService::register_http_endpoint()
     │         │
     │         ▼
     │    QodRestAdapter (af_core)
     │         │
     │         ├─ RestRouter: HTTP → Message
     │         ▼
     │    RequestRouter → QodHandler
     │
     └─ gRPC: InternalCommunication/SendMessage (unchanged)
               │
               ▼
          RequestRouter → QodHandler
```

### Component Responsibilities

#### Transport Layer (common/communication/)

**`CommunicationService::register_http_endpoint()`**
- New virtual method for registering REST endpoints
- Default implementation returns `false` (not supported by gRPC/Direct)
- HTTP implementation provides method + path filtering

**`HttpCommunicationService::register_http_endpoint()`**
- Wraps handler with HTTP method filtering
- Delegates to server transport route registration
- Returns 405 Method Not Allowed for wrong HTTP method

#### Application Layer (af_core/)

**`RestRouter`** (`af_core/include/rest_router.h`)
- Generic, reusable HTTP ↔ Message converter
- Shared across all CAMARA services (QoD, Device Location, etc.)
- Responsibilities:
  - Extract path parameters (e.g., `{sessionId}`)
  - Map HTTP headers to `Message.metadata`
  - Convert HTTP body to `Message.payload`
  - Convert `Message` response to HTTP response
  - Generate or propagate correlation IDs

**Service-Specific REST Adapters** (e.g., `af_core/include/qod/qod_rest_adapter.h`)
- Lives alongside service components (handler, session manager, etc.)
- Registers CAMARA endpoints with HTTP service
- Creates handlers that:
  1. Convert HTTP request → Message
  2. Route to internal handler via RequestRouter
  3. Convert Message response → HTTP response

### CAMARA QoD Example

**Endpoint Registration** (in `QodRestAdapter::register_endpoints()`):

```cpp
// POST /quality-on-demand/v1/sessions → qod_create_session
http_service->register_http_endpoint(
    "POST",
    "/quality-on-demand/v1/sessions",
    create_handler("qod_create_session", "/quality-on-demand/v1/sessions"));

// GET /quality-on-demand/v1/sessions/{sessionId} → qod_get_session
http_service->register_http_endpoint(
    "GET",
    "/quality-on-demand/v1/sessions/*",
    create_handler("qod_get_session", "/quality-on-demand/v1/sessions/{sessionId}"));
```

**Request Example**:

```bash
curl -X POST http://af_core:8080/quality-on-demand/v1/sessions \
  -H 'content-type: application/json' \
  -d '{
    "device": {"phoneNumber": "+123456789"},
    "applicationServer": {"ipv4Address": "0.0.0.0/0"},
    "qosProfile": "premium",
    "duration": 3600
  }'
```

**Response Example**:

```json
{
  "sessionId": "123e4567-e89b-12d3-a456-426614174000",
  "qosStatus": "REQUESTED",
  "duration": 3600,
  "applicationServer": {"ipv4Address": "0.0.0.0/0"},
  "qosProfile": "premium"
}
```

### File Organization

The REST routing components are organized to support multiple CAMARA services:

```
af_core/
  include/
    rest_router.h              # Generic, shared across all services
    qod/
      qod_rest_adapter.h       # QoD-specific adapter
    device_location/           # Future service
      location_rest_adapter.h
  src/
    rest_router.cpp            # Generic implementation
    qod/
      qod_rest_adapter.cpp     # QoD-specific implementation
    device_location/           # Future service
      location_rest_adapter.cpp
```

**Rationale:**
- `RestRouter` at root level: Generic utility shared by all services
- Service adapters in service directories: Co-located with related components
- Each CAMARA API (QoD, Device Location, SIM Swap) implements its own adapter

### Endpoint Mapping Table

Current CAMARA QoD API endpoints:

| HTTP Method | Path | Internal Message Type |
|-------------|------|----------------------|
| `POST` | `/quality-on-demand/v1/sessions` | `qod_create_session` |
| `GET` | `/quality-on-demand/v1/sessions/{sessionId}` | `qod_get_session` |
| `DELETE` | `/quality-on-demand/v1/sessions/{sessionId}` | `qod_delete_session` |
| `POST` | `/quality-on-demand/v1/sessions/{sessionId}/extend` | `qod_extend_session` |
| `POST` | `/quality-on-demand/v1/retrieve-sessions` | `qod_retrieve_sessions` |

### Path Parameter Extraction

The `RestRouter` extracts path parameters using regex pattern matching:

```cpp
// Pattern: "/quality-on-demand/v1/sessions/{sessionId}"
// Path:    "/quality-on-demand/v1/sessions/123e4567-e89b-12d3-a456-426614174000"
// Result:  {"sessionId": "123e4567-e89b-12d3-a456-426614174000"}
```

Parameters are added to `Message.metadata` for handler access:

```cpp
auto session_id = message->metadata["sessionId"];
```

### Message Flow Example

1. **HTTP Request arrives**: `POST /quality-on-demand/v1/sessions`
2. **HTTP Server Transport**: Routes to registered handler for path
3. **QodRestAdapter Handler**: Receives HttpRequest
4. **RestRouter**: Converts to Message:
   ```cpp
   message->message_type = "qod_create_session"
   message->payload = req.body (JSON bytes)
   message->metadata["content-type"] = "application/json"
   message->metadata["x-correlator"] = "..." (from header or generated)
   ```
5. **RequestRouter**: Dispatches to `QodHandler::handle_create_session()`
6. **QodHandler**: Processes request, returns Message response
7. **RestRouter**: Converts Message → HttpServerResponse:
   ```cpp
   response.status_code = message->metadata["status"]
   response.body = message->payload
   response.headers["x-correlator"] = message->correlation_id
   ```
8. **HTTP Server Transport**: Sends response to client

### Backward Compatibility

- ✅ **gRPC**: Completely unchanged, continues using message_type routing
- ✅ **Existing Handlers**: No modifications needed (work with Message objects)
- ✅ **Configuration**: HTTP vs gRPC remains a configuration choice
- ✅ **Change**: `/internal/messages` HTTP endpoint removed completely
  - Use gRPC or Direct transport for internal AF component communication
  - Use CAMARA REST API for external client communication
  - Use send_http() for outbound calls to 5G Core network functions
  - Internal AF component communication continues using message envelopes
  - External clients must use CAMARA REST endpoints

### Initialization Flow

**In AfOrchestrator** (when using HTTP transport):

```cpp
void AfOrchestrator::initialize_communication() {
    // ... create main HTTP service ...

    // Initialize REST endpoints if using HTTP transport
    if (core_comm.kind == CommunicationKind::Http) {
        initialize_rest_endpoints();
    }
}

void AfOrchestrator::initialize_rest_endpoints() {
    // Create shared REST router
    rest_router_ = std::make_shared<RestRouter>();

    // Create QoD REST adapter
    qod_rest_adapter_ = std::make_shared<qod::QodRestAdapter>(
        rest_router_, request_router_);

    // Register CAMARA QoD endpoints
    auto main_comm = communication_services_["main"];
    qod_rest_adapter_->register_endpoints(main_comm);
}
```

---

## Communication Links in Detail

### 1. External Client → AF Core (Northbound Inbound - REST API)

```
┌──────────────┐    HTTP/2 POST /quality-on-demand/v1/sessions    ┌──────────────┐
│ External     │ ──────────────────────────────────────────────► │   AF Core    │
│ Client       │ ◄────────────────────────────────────────────── │   (Server)   │
│ (CAMARA)     │           JSON response (CAMARA format)         └──────────────┘
└──────────────┘
```

**External Client side**:
- Sends standard CAMARA REST API requests
- No message envelope required
- Direct JSON payloads per CAMARA specification

**AF Core side** (server):
- Runs `Http2ServerTransport` listening on configured port (e.g., `8080`)
- Routes by HTTP method + path to REST adapters
- REST adapter converts to internal Message format
- Handler processes and returns Message
- REST router converts back to HTTP response

**Configuration example:**

```yaml
# AF Core
af_core:
  communication:
    kind: http
    listen:
      host: "0.0.0.0"
      port: 8080
```

### 2. Adapter → AF Core (Northbound Inbound - Internal Message)

**Note:** If using custom adapters that need message-type routing, they should migrate to CAMARA REST endpoints or use gRPC transport.

---

### 3. AF Core ↔ PCF Handler (Southbound Internal — Bidirectional)

```
┌──────────────┐  ── pcf client (request) ──────────────►  ┌──────────────┐
│   AF Core    │                                           │ PCF Handler  │
│  (port 8080) │  ◄── pcf_handler client (notification) ── │  (port 8085) │
└──────────────┘                                           └──────────────┘
```

This link is **bidirectional** and uses the **internal message envelope** (not REST). AF Core sends requests to PCF Handler, and PCF Handler sends notifications back to AF Core. They each use separate communication service instances:

| Component | Service | Mode | Purpose |
|-----------|---------|------|---------|
| AF Core | `communication_services_["main"]` | Server + Client | Accepts inbound from external clients **and** southbound handlers |
| AF Core | `communication_services_["pcf"]` | Client-only | Sends outbound requests to PCF Handler |
| PCF Handler | `core_comm_` | Server + Client | Accepts inbound from AF Core **and** sends notifications back |

#### Request flow (AF Core → PCF Handler)

AF Core uses its client-only `"pcf"` service to send messages like `pcf_create_qod_session` to PCF Handler's server.

#### Notification flow (PCF Handler → AF Core)

When the 5G PCF sends a notification (e.g., UE state change, session termination), PCF Handler:

1. Receives the event from 5G Core via its PCF SBI subscription callback
2. Builds an internal `Message` (e.g., `message_type: "pcf_notification"`)
3. Sends it to AF Core's **main server** using PCF Handler's own outbound client (configured via `remote`)

AF Core's main server dispatches it to the registered notification handler, which updates `QodSessionManager`, fires events, etc.

> **Key insight:** AF Core receives southbound notifications through its **main server**, not through the PCF client connection. The `"pcf"` client is request-only (ask PCF Handler to do something). Notifications flow the other way — PCF Handler initiates a message to AF Core's server.

**This link uses the same internal message envelope** as AF Core ↔ PCF Handler internal communication. The transport is symmetric — both directions use the internal message format with JSON `Message` bodies.

**Configuration example:**

```yaml
af_core:
  communication:
    kind: http
    listen:
      host: "0.0.0.0"
      port: 8080

pcf_handler:
  communication:
    kind: http
    listen:
      host: "0.0.0.0"
      port: 8085
    remote:                    # Client back to AF Core (for notifications)
      host: "af_core"
      port: 8080
```

---

### 4. PCF Handler → 5G Core PCF (Southbound External)

```
┌──────────────┐     HTTP/2 (Npcf_PolicyAuthorization)      ┌──────────────┐
│ PCF Handler  │ ──────────────────────────────────────────► │   5G PCF     │
│  (Gateway)   │ ◄────────────────────────────────────────── │  (3GPP NF)   │
└──────────────┘          Standard HTTP/2 API                └──────────────┘
```

**This link does NOT use the internal message envelope.**

It uses the extended `send_http()` method on `HttpCommunicationService` to make standard HTTP/2 API calls:

```cpp
// Raw HTTP/2 request to PCF
auto response = http_service->send_http(
    "POST",
    "/npcf-policyauthorization/v1/app-sessions",
    {{"content-type", "application/json"}},
    pcf_request_body
);
```

The mapping from AF operations to PCF HTTP endpoints:

| AF Operation | HTTP Method | PCF Path |
|-------------|-------------|----------|
| Create App Session | `POST` | `/npcf-policyauthorization/v1/app-sessions` |
| Update App Session | `PATCH` | `/npcf-policyauthorization/v1/app-sessions/{id}` |
| Delete App Session | `POST` | `/npcf-policyauthorization/v1/app-sessions/{id}/delete` |
| Get App Session | `GET` | `/npcf-policyauthorization/v1/app-sessions/{id}` |

This is mediated by a **PcfGateway** abstraction that encapsulates the route mapping, keeping domain logic clean:

```cpp
class PcfGateway {
public:
    virtual std::pair<bool, json> create_app_session(const json& context) = 0;
    virtual std::pair<bool, json> update_app_session(const string& id, const json& data) = 0;
    virtual bool delete_app_session(const string& id, const json& data) = 0;
    virtual std::pair<bool, json> get_app_session(const string& id) = 0;
};
```

The `HttpPcfGateway` implementation uses the DI-managed `CommunicationService` (configured with the PCF base URL) internally.

---

## Integration with Dependency Injection

The HTTP transport integrates with DI identically to gRPC — no DI framework changes are required.

### Registration Flow

```
YAML config (kind: http)
    │
    ▼
CommunicationKind::Http  (af_typed_config.hpp)
    │
    ▼
to_string() → "http"
    │
    ▼
CommunicationFactory::create_service("http", name, config)
    │
    ▼
HttpCommunicationService  (instantiated and initialized)
    │
    ▼
Stored in communication_services_ map  (AfOrchestrator / PcfHandler)
```

### Named Service Instances

Multiple HTTP services can coexist with different roles:

```cpp
// In AfOrchestrator
communication_services_["main"]   // Server + Client: accepts from adapters AND southbound handlers
communication_services_["pcf"]    // Client-only: sends requests to PCF Handler

// In PcfHandler
core_comm_                        // Server + Client: accepts from AF Core AND sends notifications back
pcf_gateway_                      // Client-only: sends raw HTTP/2 to external PCF (via send_http)
```

> **Note:** AF Core's `"main"` service is the single entry point for all inbound messages — from northbound adapters *and* from southbound handlers sending notifications. The `"pcf"` client is used only for outbound request/response calls.

### Configuration-Driven Transport Selection

Switching between gRPC and HTTP requires only a YAML change:

```yaml
# Use gRPC
af_core:
  communication:
    kind: grpc
    listen:
      host: "0.0.0.0"
      port: 50051

# Use HTTP/2
af_core:
  communication:
    kind: http
    listen:
      host: "0.0.0.0"
      port: 8080
```

No code changes are needed in business logic, handlers, or routing.

---

## File Layout

```
common/communication/implementations/http/
├── CMakeLists.txt                      # Build: nghttp2, Boost, OpenSSL, nlohmann_json
├── include/
│   ├── http2_client_transport.h        # Reusable HTTP/2 client
│   ├── http2_server_transport.h        # Reusable HTTP/2 server
│   └── http_communication_service.h    # CommunicationService implementation
└── src/
    ├── http2_client_transport.cpp      # Client: connect, send, callbacks
    ├── http2_server_transport.cpp      # Server: accept, dispatch, respond
    └── http_communication_service.cpp  # Service: serialize, route, lifecycle
```

---

## nghttp2 Callback Model

Both client and server transports use nghttp2's callback-driven architecture:

### Client Callbacks

| Callback | Purpose |
|----------|---------|
| `on_header_callback` | Capture `:status` and response headers per stream |
| `on_data_chunk_recv_callback` | Accumulate response body chunks |
| `on_frame_recv_callback` | Detect END_STREAM (response complete) |
| `on_stream_close_callback` | Mark stream as done, notify waiters |

### Server Callbacks

| Callback | Purpose |
|----------|---------|
| `on_begin_headers_callback` | Allocate new request state for incoming stream |
| `on_header_callback` | Capture `:method`, `:path`, and request headers |
| `on_data_chunk_recv_callback` | Accumulate request body |
| `on_frame_recv_callback` | Detect END_STREAM, dispatch to handler, submit response |
| `on_stream_close_callback` | Clean up stream state |

---

## Connection Lifecycle

### Client

1. **Initialize** — Parse URL, create nghttp2 client session
2. **Connect** — TCP connect via Boost.Asio, send HTTP/2 SETTINGS, complete handshake
3. **Send** — Submit request frames, flush to socket, wait for response
4. **Reconnect** — Automatic on send failure (with exponential backoff)
5. **Disconnect** — Close socket, destroy nghttp2 session

### Server

1. **Initialize** — Configure listen address/port
2. **Start** — Bind acceptor, start Boost.Asio worker threads
3. **Accept** — Per-connection: create nghttp2 server session, send SETTINGS
4. **Serve** — Read frames, dispatch completed requests to handlers, respond
5. **Stop** — Close acceptor, terminate connections, join threads

---

## Error Handling

| Scenario | Behavior |
|----------|----------|
| Connection refused | Client returns error response with message |
| Request timeout | Client returns error after configured `timeout_ms` |
| Invalid JSON in request | Server returns HTTP 400 |
| No handler for message_type | Server returns error `Message` with diagnostic |
| Connection dropped mid-stream | Stream marked as error, client can retry |
| Server overloaded | HTTP/2 flow control + `MAX_CONCURRENT_STREAMS` setting |

---

## Future Considerations

### REST API Enhancements

- **OpenAPI Spec Generation**: Auto-generate OpenAPI 3.0 spec from registered endpoints, serve at `/openapi.json`
- **Path Parameter Validation**: Add regex validation for UUID format in `{sessionId}`, return 400 for malformed parameters
- **Content Negotiation**: Support `application/json` and `application/xml`, use `Accept` header for response format
- **Rate Limiting**: Add per-endpoint rate limiting, return 429 Too Many Requests when exceeded
- **Additional CAMARA APIs**: Add Device Location, SIM Swap, etc. following the same RestAdapter pattern

### Transport Features

- **TLS/ALPN**: Currently cleartext HTTP/2 with prior knowledge. TLS support is implemented but gated behind config (`use_tls: true`).
- **Subscriptions**: `subscribe()`/`unsubscribe()` are not supported over HTTP. If needed, Server-Sent Events (SSE) or webhook callbacks could be added later.
- **Health checks**: The server could expose `GET /health` for liveness probes.
- **Metrics**: Request count, latency, and error rate could be emitted via the observability framework.
