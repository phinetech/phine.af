# Demo QoD Adapter — Design Document

## 1. Overview

This document describes the design for a **demo C++ adapter application** that acts as the *Application Management* component in a ROS2-over-5G deployment. The adapter demonstrates how to request and manage CAMARA Quality-on-Demand (QoD) sessions through phine.af for different ROS2 traffic types.

### 1.1 Context: ROS2 with 5G Integration

Refer to the [phine.af architecture diagram](../README.md#architecture-diagram) for the full picture.

- **ROS2 nodes as UEs** — onboard nodes on a mobile robot connected to the 5G RAN.
- **ROS2 nodes as DN Applications** — cloud-hosted ROS2 nodes accessible via the UPF.
- **Application Management** — this adapter — coordinates QoS requirements for the robot's communication streams.

### 1.2 Scope

This version focuses exclusively on the **Application Management ↔ phine.af** interaction:

| In scope | Out of scope |
|---|---|
| Request QoS for different traffic types via QoD API | Actual ROS2 node integration |
| Session lifecycle management (create, monitor, cleanup) | Real traffic generation/simulation |
| Configuration-driven QoS requests | ROS2 topic/service discovery |
| Reusable C++ QoD API client class | Authentication/authorization (OAuth2) |

Future versions can extend this to integrate with actual ROS2 nodes and demonstrate end-to-end QoS enforcement.

---

## 2. Simulation Scenario

The adapter simulates managing QoS for a single mobile robot whose UE IP is `10.60.0.1`. Three distinct traffic flows need different QoS guarantees:

| # | Stream | Port(s) | QoS Profile | Rationale |
|---|--------|---------|-------------|-----------|
| 1 | **Video Stream** | 8554 (RTSP) | `QOS_L` (high bandwidth) | HD camera feed for remote monitoring/processing |
| 2 | **WebRTC Control** | 8443 | `QOS_E` (low latency) | Real-time teleoperation commands and status |
| 3 | **General Traffic** | all remaining | `QOS_S` (best-effort) | Sensor data, logs, diagnostics |

The adapter creates one QoD session per stream, monitors their status, and cleans them up on exit.

---

## 3. Architecture

### 3.1 Component Placement

```
┌─────────────────────────────────┐
│  demo-qod-adapter (this app)    │
│  "Application Management"       │
│                                 │
│  ┌────────────┐ ┌─────────────┐ │       gRPC (InternalMessage)       ┌──────────────┐
│  │SessionMgr  │→│ QodClient   │─┼──────────────────────────────────→│  af_core      │
│  └────────────┘ └─────────────┘ │       :50051                      │  (phine.af)   │
│        ↑                        │                                    └──────┬───────┘
│   config.yaml                   │                                           │
└─────────────────────────────────┘                                     PCF / NEF
```

The adapter is a standalone process (container) that connects to `af_core` over the existing gRPC `InternalCommunication` service. It does **not** link against the `af_common` library at compile time — it only depends on the generated protobuf stubs for `message.proto`.

### 3.2 Interaction with phine.af

All QoD operations are sent as `af::proto::InternalMessage` RPCs to af_core's gRPC server at port `50051`. Payloads are JSON-serialized CAMARA QoD request/response bodies carried in the `bytes payload` field.

#### Message Types (registered in af_core's `RequestRouter`)

| `message_type` | CAMARA Equivalent | Payload (JSON) | Metadata |
|---|---|---|---|
| `qod_create_session` | `POST /sessions` | `CreateSession` body | `source: demo-qod-adapter` |
| `qod_get_session` | `GET /sessions/{id}` | *(empty)* | `session_id: <uuid>` |
| `qod_delete_session` | `DELETE /sessions/{id}` | *(empty)* | `session_id: <uuid>` |
| `qod_extend_session` | `POST /sessions/{id}/extend` | `ExtendSessionDuration` body | `session_id: <uuid>` |
| `qod_retrieve_sessions` | `POST /retrieve-sessions` | `RetrieveSessionsInput` body | — |

#### Example: Create Session Payload

```json
{
  "device": {
    "ipv4Address": {
      "publicAddress": "10.60.0.1",
      "publicPort": 8554
    }
  },
  "applicationServer": {
    "ipv4Address": "0.0.0.0/0"
  },
  "devicePorts": {
    "ports": [8554]
  },
  "qosProfile": "QOS_L",
  "duration": 3600
}
```

#### Example: Session Response

```json
{
  "sessionId": "3fa85f64-5717-4562-b3fc-2c963f66afa6",
  "qosStatus": "REQUESTED",
  "duration": 3600,
  "qosProfile": "QOS_L",
  "applicationServer": { "ipv4Address": "0.0.0.0/0" }
}
```

### 3.3 Wire Protocol (gRPC)

The adapter uses the `InternalCommunication.SendMessage` RPC defined in `common/protos/message.proto`:

```protobuf
service InternalCommunication {
  rpc SendMessage(InternalMessage) returns (InternalMessage);
}

message InternalMessage {
  string message_type = 1;
  string correlation_id = 2;
  bytes payload = 3;
  map<string, string> metadata = 4;
}
```

JSON payloads are serialized to raw bytes (no base64). Responses are deserialized the same way:

```cpp
// Sending
std::string json = body.dump();
request.set_payload(json.data(), json.size());

// Receiving
std::string resp_json(response.payload().begin(), response.payload().end());
auto result = nlohmann::json::parse(resp_json);
```

---

## 4. Directory Layout

```
adapters/
└── demo-qod-adapter/
    ├── CMakeLists.txt            # Build configuration; depends on gRPC, protobuf, nlohmann_json, spdlog
    ├── Dockerfile                # Multi-stage build (grpc-builder → build → runtime)
    ├── DESIGN.md                 # This document
    ├── README.md                 # Quick-start, build & run instructions
    ├── config.yaml               # Demo configuration (UE IP, streams, af_core endpoint)
    ├── include/
    │   ├── qod_client.hpp        # Low-level gRPC client for QoD operations
    │   └── session_manager.hpp   # High-level session lifecycle orchestration
    ├── src/
    │   ├── main.cpp              # Entry point — loads config, runs demo lifecycle
    │   ├── qod_client.cpp        # QodClient implementation
    │   └── session_manager.cpp   # SessionManager implementation
    └── tests/
        ├── CMakeLists.txt        # Test build configuration
        ├── test_qod_client.cpp   # Unit tests for QodClient (mocked gRPC channel)
        └── test_session_manager.cpp  # Unit tests for SessionManager (mocked QodClient)
```

---

## 5. Detailed Class Design

### 5.1 `QodClient` — Low-Level API Client

**File:** `include/qod_client.hpp`, `src/qod_client.cpp`

**Responsibility:** Encapsulates all gRPC communication with af_core for QoD operations. Handles connection lifecycle, request serialization, response deserialization, and error mapping.

```cpp
namespace phine::adapter {

struct QodClientConfig {
    std::string af_core_address;  // e.g. "192.168.70.141:50051"
    int timeout_seconds = 30;
    int max_retries = 3;
    int retry_delay_ms = 1000;
};

// Mirrors CAMARA QoD data models (simplified for the adapter)
struct DeviceIpv4Addr {
    std::string public_address;
    int public_port = 0;
};

struct Device {
    std::optional<std::string> phone_number;
    std::optional<DeviceIpv4Addr> ipv4_address;
};

struct ApplicationServer {
    std::string ipv4_address;
};

struct PortsSpec {
    std::vector<int> ports;
    // ranges omitted for simplicity; can be added
};

struct CreateSessionRequest {
    Device device;
    ApplicationServer application_server;
    std::optional<PortsSpec> device_ports;
    std::string qos_profile;
    int duration_seconds;
    std::optional<std::string> sink;       // notification callback URL
};

enum class QosStatus { REQUESTED, AVAILABLE, UNAVAILABLE };
enum class StatusInfo { NONE, DURATION_EXPIRED, NETWORK_TERMINATED, DELETE_REQUESTED };

struct SessionInfo {
    std::string session_id;
    QosStatus qos_status;
    std::optional<StatusInfo> status_info;
    int duration;
    std::string qos_profile;
    std::optional<std::string> started_at;
    std::optional<std::string> expires_at;
};

struct ExtendSessionRequest {
    int requested_additional_duration;
};

// Result wrapper
template <typename T>
struct Result {
    bool success;
    T value;
    std::string error_code;
    std::string error_message;
};

class QodClient {
public:
    explicit QodClient(const QodClientConfig& config);
    ~QodClient();

    /// Block until the gRPC channel is connected or timeout expires.
    bool wait_for_ready(int timeout_seconds = 30);

    /// POST /sessions
    Result<SessionInfo> create_session(const CreateSessionRequest& request);

    /// GET /sessions/{sessionId}
    Result<SessionInfo> get_session(const std::string& session_id);

    /// DELETE /sessions/{sessionId}
    Result<void> delete_session(const std::string& session_id);

    /// POST /sessions/{sessionId}/extend
    Result<SessionInfo> extend_session(const std::string& session_id,
                                       const ExtendSessionRequest& request);

    /// POST /retrieve-sessions
    Result<std::vector<SessionInfo>> retrieve_sessions(const Device& device);

private:
    QodClientConfig config_;
    std::shared_ptr<grpc::Channel> channel_;
    std::unique_ptr<af::proto::InternalCommunication::Stub> stub_;

    /// Send an InternalMessage and return the response (with retry logic).
    Result<std::string> send_message(const std::string& message_type,
                                     const std::string& json_payload,
                                     const std::map<std::string, std::string>& metadata);

    /// Generate a unique correlation ID.
    static std::string generate_correlation_id();
};

} // namespace phine::adapter
```

#### Key Implementation Details

1. **Connection management** — A single `grpc::Channel` is created at construction pointing to `af_core_address`. `wait_for_ready()` polls `channel_->GetState()` until `GRPC_CHANNEL_READY`.

2. **Retry logic** — `send_message()` retries on transient gRPC errors (`UNAVAILABLE`, `DEADLINE_EXCEEDED`) up to `max_retries` times with exponential backoff starting at `retry_delay_ms`.

3. **Serialization** — Uses `nlohmann::json` to build request payloads from the request structs and parse response payloads into `SessionInfo`. The JSON keys follow CAMARA camelCase conventions (`sessionId`, `qosProfile`, `qosStatus`, etc.).

4. **Error handling** — gRPC-level errors are mapped to `Result.error_code = "GRPC_<status>"`. af_core-level errors (returned as JSON in the response payload) are parsed and propagated as CAMARA error codes (e.g., `QUALITY_ON_DEMAND.QOS_PROFILE_NOT_FOUND`).

5. **Correlation IDs** — Each request gets a UUID-v4 correlation ID for tracing.

### 5.2 `SessionManager` — High-Level Lifecycle Manager

**File:** `include/session_manager.hpp`, `src/session_manager.cpp`

**Responsibility:** Manages the lifecycle of multiple QoD sessions. Reads stream definitions from configuration, creates sessions via `QodClient`, tracks their state, provides polling-based monitoring, and ensures cleanup on shutdown.

```cpp
namespace phine::adapter {

struct StreamConfig {
    std::string name;             // e.g. "video_stream"
    std::string description;      // Human-readable description
    Device device;                // UE identity
    ApplicationServer app_server; // DN application
    std::optional<PortsSpec> device_ports;
    std::string qos_profile;      // CAMARA profile name
    int duration_seconds;
};

struct TrackedSession {
    StreamConfig stream;
    std::optional<SessionInfo> session_info;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_checked;
    int check_count = 0;
    bool cleanup_done = false;
};

class SessionManager {
public:
    explicit SessionManager(std::shared_ptr<QodClient> client);
    ~SessionManager();

    /// Create a QoD session for a configured stream.
    Result<SessionInfo> request_qos_for_stream(const StreamConfig& stream);

    /// Query the current status of a tracked session.
    Result<SessionInfo> get_session_status(const std::string& session_id);

    /// Delete a specific session and remove it from tracking.
    Result<void> delete_session(const std::string& session_id);

    /// Create sessions for all configured streams.
    void create_all_sessions(const std::vector<StreamConfig>& streams);

    /// Poll all tracked sessions and log status changes.
    void monitor_sessions();

    /// Delete all tracked sessions (graceful shutdown).
    void cleanup_all_sessions();

    /// Get all currently tracked sessions.
    const std::map<std::string, TrackedSession>& get_tracked_sessions() const;

private:
    std::shared_ptr<QodClient> client_;
    std::map<std::string, TrackedSession> sessions_;  // keyed by session_id
    spdlog::logger logger_;
};

} // namespace phine::adapter
```

#### Key Implementation Details

1. **Session tracking** — Each successfully created session is stored in `sessions_` keyed by `session_id`. The `TrackedSession` struct records the original stream config, last-known `SessionInfo`, timestamps, and check count.

2. **Monitoring** — `monitor_sessions()` iterates all tracked sessions, calls `client_->get_session(id)`, logs any status transitions (e.g., `REQUESTED → AVAILABLE`), and updates `TrackedSession.session_info`.

3. **Cleanup** — `cleanup_all_sessions()` iterates all tracked sessions and calls `client_->delete_session(id)` for each. Failed deletions are logged but do not block cleanup of remaining sessions.

4. **Logging** — Uses `spdlog` for structured logging. Key log points:
   - Session creation request and response
   - Status changes during monitoring
   - Errors with full CAMARA error codes
   - Cleanup progress

### 5.3 `main.cpp` — Demo Entry Point

**Responsibility:** Loads configuration, wires components, and runs the demo lifecycle.

```
main()
  ├── Load config.yaml
  ├── Create QodClient(config)
  ├── Wait for af_core readiness
  ├── Create SessionManager(client)
  ├── Create sessions for all 3 streams
  ├── Monitor loop (poll every N seconds, or for M iterations)
  ├── Cleanup all sessions
  └── Exit
```

#### Demo Flow (pseudocode)

```
1. Parse config.yaml → QodClientConfig + vector<StreamConfig>
2. QodClient client(config)
3. client.wait_for_ready(30)      // block until af_core is reachable
4. SessionManager mgr(client)
5. mgr.create_all_sessions(streams)
6. for (int i = 0; i < monitor_iterations; ++i):
       sleep(monitor_interval)
       mgr.monitor_sessions()
7. mgr.cleanup_all_sessions()
8. Log summary: sessions created, final statuses, errors encountered
```

---

## 6. Configuration

**File:** `config.yaml`

```yaml
# af_core connection
af_core:
  address: "192.168.70.141:50051"
  timeout_seconds: 30

# Retry policy
retry:
  max_retries: 3
  initial_delay_ms: 1000

# Monitoring
monitor:
  interval_seconds: 10
  iterations: 6         # monitor for ~60 seconds total

# Stream definitions — the demo's QoD session requests
streams:
  - name: "video_stream"
    description: "HD camera feed for remote monitoring/processing"
    device:
      ipv4_address:
        public_address: "10.60.0.1"
        public_port: 8554
    application_server:
      ipv4_address: "0.0.0.0/0"
    device_ports:
      ports: [8554]
    qos_profile: "QOS_L"
    duration_seconds: 3600

  - name: "webrtc_control"
    description: "Real-time teleoperation commands and status"
    device:
      ipv4_address:
        public_address: "10.60.0.1"
        public_port: 8443
    application_server:
      ipv4_address: "0.0.0.0/0"
    device_ports:
      ports: [8443]
    qos_profile: "QOS_E"
    duration_seconds: 3600

  - name: "general_traffic"
    description: "Sensor data, logs, diagnostics"
    device:
      ipv4_address:
        public_address: "10.60.0.1"
        public_port: 0           # any port
    application_server:
      ipv4_address: "0.0.0.0/0"
    qos_profile: "QOS_S"
    duration_seconds: 3600
```

---

## 7. Build & Dependencies

### 7.1 Dependencies

| Dependency | Purpose | Version |
|---|---|---|
| gRPC + Protobuf | Communication with af_core | Matching `phinetech/grpc-builder:v1.72.2` |
| nlohmann/json | JSON serialization/deserialization | ≥ 3.11 |
| spdlog + fmt | Structured logging | System packages |
| yaml-cpp | Configuration parsing | System packages |
| Google Test | Unit testing | ≥ 1.14 |

### 7.2 CMakeLists.txt Strategy

The adapter builds as a **standalone executable** with minimal coupling to the phine.af codebase:

1. **Generated protobuf stubs** — Reuse the `af_protos` library from `common/protos/` (either as a pre-built artifact or by including the proto file and generating stubs locally).
2. **No link to af_common** — The adapter defines its own request/response structs and JSON serialization. This keeps it portable and independent.
3. **Test target** — Separate executable linking Google Test and the adapter's library objects.

```cmake
cmake_minimum_required(VERSION 3.14)
project(demo_qod_adapter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# --- Dependencies ---
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)  # or via FindgRPC.cmake
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)

# --- Proto stubs (reuse from build-output or generate) ---
# Option A: Link pre-built af_protos
# Option B: Generate from common/protos/message.proto locally
add_library(adapter_protos ...)

# --- Adapter library (for testing) ---
add_library(adapter_lib STATIC
    src/qod_client.cpp
    src/session_manager.cpp
)
target_link_libraries(adapter_lib PUBLIC
    adapter_protos
    gRPC::grpc++
    protobuf::libprotobuf
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    yaml-cpp
)

# --- Main executable ---
add_executable(demo_qod_adapter src/main.cpp)
target_link_libraries(demo_qod_adapter PRIVATE adapter_lib)

# --- Tests ---
enable_testing()
find_package(GTest REQUIRED)
add_executable(adapter_tests
    tests/test_qod_client.cpp
    tests/test_session_manager.cpp
)
target_link_libraries(adapter_tests PRIVATE adapter_lib GTest::gtest_main)
add_test(NAME adapter_tests COMMAND adapter_tests)
```

### 7.3 Dockerfile

Multi-stage build following the same pattern as `af_core/Dockerfile`:

```
Stage 1: grpc-builder      — phinetech/grpc-builder:v1.72.2 (pre-built gRPC/protobuf)
Stage 2: dependencies       — debian:bookworm-slim + build tools + system libs
Stage 3: build              — compile adapter (cmake, make)
Stage 4: runtime            — minimal image with just the binary + config + shared libs
```

### 7.4 Docker Compose Integration

Add the adapter to `docker-compose/docker-compose-build.yaml`:

```yaml
  demo_qod_adapter:
    image: tariromukute/demo-qod-adapter:${TAG:-latest}
    container_name: demo-qod-adapter
    build:
      context: ..
      dockerfile: adapters/demo-qod-adapter/Dockerfile
    volumes:
      - ../adapters/demo-qod-adapter/config.yaml:/app/config.yaml
    depends_on:
      - af_core
    networks:
      af_net:
        ipv4_address: 192.168.70.143
```

---

## 8. Error Handling Strategy

### 8.1 Error Classification

| Category | Examples | Handling |
|---|---|---|
| **Transport errors** | gRPC `UNAVAILABLE`, `DEADLINE_EXCEEDED` | Retry with exponential backoff |
| **Client errors** | CAMARA `400 INVALID_ARGUMENT`, `422 MISSING_IDENTIFIER` | Log, do not retry (config issue) |
| **Not found** | CAMARA `404 NOT_FOUND` | Log, remove from tracking |
| **Conflict** | CAMARA `409 CONFLICT` | Log, session already exists for device |
| **Server errors** | gRPC `INTERNAL`, `UNKNOWN` | Retry (may be transient) |

### 8.2 Retry Policy

```
for attempt in 1..max_retries:
    response = stub->SendMessage(request)
    if response.ok():
        return parse(response)
    if is_retryable(response.error_code()):
        delay = initial_delay_ms * 2^(attempt-1)
        sleep(delay)
        continue
    else:
        return error(response)
return error("max retries exceeded")
```

Retryable gRPC status codes: `UNAVAILABLE`, `DEADLINE_EXCEEDED`, `RESOURCE_EXHAUSTED`.

### 8.3 Graceful Shutdown

The adapter registers a `SIGINT`/`SIGTERM` handler that triggers `cleanup_all_sessions()` before exit. This ensures QoD sessions are not orphaned in af_core.

---

## 9. Logging

All logging uses `spdlog` with the following convention:

| Level | Usage |
|---|---|
| `INFO` | Session lifecycle events (created, status change, deleted) |
| `DEBUG` | Full JSON request/response payloads |
| `WARN` | Retriable errors, unexpected status transitions |
| `ERROR` | Non-retriable failures, cleanup failures |

Example output:

```
[2026-02-16 10:00:01.123] [demo-qod-adapter] [info] Creating session for stream 'video_stream' (QOS_L, port 8554)
[2026-02-16 10:00:01.456] [demo-qod-adapter] [info] Session created: id=3fa85f64-..., status=REQUESTED
[2026-02-16 10:00:11.789] [demo-qod-adapter] [info] Session 3fa85f64-... status changed: REQUESTED → AVAILABLE
[2026-02-16 10:01:01.012] [demo-qod-adapter] [info] Cleaning up session 3fa85f64-... (video_stream)
[2026-02-16 10:01:01.234] [demo-qod-adapter] [info] Session 3fa85f64-... deleted successfully
```

---

## 10. Testing Strategy

### 10.1 Unit Tests

| Test file | What is tested | Mocking approach |
|---|---|---|
| `test_qod_client.cpp` | Request serialization, response parsing, error handling, retry logic | Mock gRPC stub (inject a fake `InternalCommunication::Stub`) |
| `test_session_manager.cpp` | Session lifecycle, tracking, monitoring, cleanup | Mock `QodClient` (inject via `shared_ptr<QodClient>` or interface) |

#### Key Test Cases — QodClient

- `CreateSession_Success` — valid request produces correct JSON payload and parses response
- `CreateSession_InvalidProfile` — CAMARA 422 error is correctly mapped
- `GetSession_NotFound` — CAMARA 404 maps to `Result.success = false`
- `DeleteSession_Success` — 204-equivalent response is handled
- `Retry_OnTransientError` — `UNAVAILABLE` triggers retries up to max
- `Retry_ExhaustsAttempts` — returns error after `max_retries`
- `Serialization_AllFields` — optional fields are included/omitted correctly

#### Key Test Cases — SessionManager

- `CreateAllSessions_Success` — 3 streams → 3 tracked sessions
- `CreateAllSessions_PartialFailure` — 1 fails, 2 succeed, failed one is logged
- `MonitorSessions_DetectsStatusChange` — status transition triggers log
- `CleanupAllSessions_Success` — all sessions deleted, tracking cleared
- `CleanupAllSessions_PartialFailure` — failures logged, remaining sessions still cleaned up

### 10.2 Testability Design

To support mocking, `QodClient` will either:

1. Accept a `grpc::Channel` in its constructor (allowing injection of a mock channel), or
2. Implement an `IQodClient` interface that `SessionManager` depends on

**Recommended approach:** Option 2 (interface-based) for cleaner tests:

```cpp
class IQodClient {
public:
    virtual ~IQodClient() = default;
    virtual Result<SessionInfo> create_session(const CreateSessionRequest&) = 0;
    virtual Result<SessionInfo> get_session(const std::string&) = 0;
    virtual Result<void> delete_session(const std::string&) = 0;
    virtual Result<SessionInfo> extend_session(const std::string&, const ExtendSessionRequest&) = 0;
    virtual Result<std::vector<SessionInfo>> retrieve_sessions(const Device&) = 0;
};
```

`SessionManager` takes `std::shared_ptr<IQodClient>`, and tests inject a `MockQodClient`.

### 10.3 Integration Tests

A separate docker-compose file runs the adapter against a live 5G Core see [demo-adapter-tutorial]():


Verify: adapter logs show successful create → monitor → cleanup cycle, exit code 0.

---

## 11. Future ROS2 Extensibility

The design intentionally keeps the `QodClient` and `SessionManager` loosely coupled to the demo configuration, making them reusable for ROS2 integration:

| Current (demo) | Future (ROS2) |
|---|---|
| `main.cpp` reads `config.yaml` and drives lifecycle | A ROS2 node wraps `SessionManager` as a ROS2 service |
| `StreamConfig` hardcoded in YAML | `StreamConfig` derived from ROS2 topic QoS requirements |
| Single-shot create-monitor-cleanup | Event-driven: create on topic subscription, cleanup on unsubscription |
| Polling-based monitoring | Callback-based via QoD notification sink (CloudEvents) |

The `IQodClient` interface and `SessionManager` class can be compiled into a ROS2 package with minimal changes — only the entry point and configuration source change.


---

## 13. Risks & Mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| gRPC proto version mismatch | Build failure or runtime crash | Pin protobuf/gRPC versions to match `grpc-builder:v1.72.2` |
| `general_traffic` (no specific ports) may conflict with other sessions | CAMARA `409 CONFLICT` | Document as a known demo limitation; adjust port config if needed |
| CAMARA model drift | JSON fields mismatch | Freeze to the models defined in `common/models/qod/qod_session.h` |
