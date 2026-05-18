# Demo QoD Adapter

A C++ demo application that acts as the **Application Management** component for ROS2-over-5G deployments. It requests and manages [CAMARA Quality-on-Demand](https://github.com/camaraproject/QualityOnDemand) sessions through phine.af for different traffic types.

See [DESIGN.md](DESIGN.md) for the full design document.

## Quick Start

### Prerequisites

- CMake ≥ 3.14
- C++17 compiler
- gRPC + Protobuf (matching `phinetech/grpc-builder:v1.72.2`)
- System packages: `libspdlog-dev libyaml-cpp-dev libfmt-dev nlohmann-json3-dev`

### Build (local)

```bash
cd adapters/demo-qod-adapter
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

The binary is placed in `build/bin/demo_qod_adapter`.

### Build with tests

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_ADAPTER_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
```

### Run

```bash
./build/bin/demo_qod_adapter config.yaml
```

The adapter will:
1. Connect to af_core at the address in `config.yaml`
2. Create QoD sessions for each configured stream
3. Monitor session status changes
4. Clean up all sessions on exit (or on SIGINT/SIGTERM)

### Docker

```bash
# From repo root
docker build -f adapters/demo-qod-adapter/Dockerfile -t demo-qod-adapter .
docker run --network phine-af-net demo-qod-adapter
```

Or use docker-compose (add the service to `docker-compose/docker-compose-build.yaml`):

```yaml
demo_qod_adapter:
  image: phinetech/demo-qod-adapter:${TAG:-latest}
  container_name: demo-qod-adapter
  build:
    context: ..
    dockerfile: adapters/demo-qod-adapter/Dockerfile
  depends_on:
    - af_core
  networks:
    af_net:
      ipv4_address: 192.168.70.143
```

## Configuration

Edit `config.yaml` to change:

| Section | Key | Description |
|---------|-----|-------------|
| `af_core.address` | `host:port` | af_core gRPC endpoint |
| `af_core.timeout_seconds` | int | Connection timeout |
| `retry.max_retries` | int | Max retry attempts for transient errors |
| `retry.initial_delay_ms` | int | Initial backoff delay |
| `monitor.interval_seconds` | int | Polling interval between status checks |
| `monitor.iterations` | int | Number of monitoring cycles (use -1 for indefinite) |
| `streams[]` | list | Stream definitions (name, device, ports, QoS profile, duration) |

## Architecture

```
┌─────────────────────────┐       gRPC        ┌──────────────┐
│  demo-qod-adapter        │ ────────────────→ │  af_core      │
│  SessionManager → QodClient │  InternalMessage │  (:50051)     │
└─────────────────────────┘                    └──────────────┘
```

The adapter sends `InternalMessage` RPCs with JSON payloads using these message types:

- `qod_create_session` — create a QoD session
- `qod_get_session` — query session status
- `qod_delete_session` — delete a session
- `qod_extend_session` — extend session duration
- `qod_retrieve_sessions` — list sessions for a device

## Project Structure

```
demo-qod-adapter/
├── CMakeLists.txt
├── Dockerfile
├── config.yaml
├── DESIGN.md
├── README.md
├── include/
│   ├── qod_client.hpp       # gRPC client (IQodClient interface + QodClient impl)
│   ├── qod_models.hpp       # Data models (Device, SessionInfo, Result<T>, etc.)
│   └── session_manager.hpp  # Session lifecycle manager
├── src/
│   ├── main.cpp
│   ├── qod_client.cpp
│   └── session_manager.cpp
└── tests/
    ├── test_qod_client.cpp
    └── test_session_manager.cpp
```
