# Demo QoD Adapter

A C++ demo application that acts as the **Application Management** component for ROS2-over-5G deployments. It requests and manages [CAMARA Quality-on-Demand](https://github.com/camaraproject/QualityOnDemand) sessions through phine.af using the CAMARA REST API.

See [DESIGN.md](DESIGN.md) for the full design document.

## Quick Start

### Prerequisites

- CMake ≥ 3.14
- C++17 compiler
- gRPC + Protobuf (matching `phinetech/grpc-builder:v1.72.2`)
- System packages: `libspdlog-dev libyaml-cpp-dev libfmt-dev nlohmann-json3-dev`

For the bundled AF + adapter build, use CMake ≥ 3.19.

### Build (local)

```bash
cd adapters/demo-qod-adapter
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

The binary is placed in `build/bin/demo_qod_adapter`.

### Build bundled AF + adapter

This adapter can also be bundled into the root `af` executable without changing the root build files.

```bash
# From repo root
cmake -S . -B build-demo-qod-bundle \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_BUNDLED=ON \
  -DCMAKE_PROJECT_phine.af_INCLUDE="$PWD/adapters/demo-qod-adapter/BundleInject.cmake"

cmake --build build-demo-qod-bundle -j$(nproc) --target af
```

The bundled binary is placed in `build-demo-qod-bundle/bin/af`.

### Build with tests

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
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

The adapter now uses the shared `af::config` loader from `common/config` for scalar configuration values, while still parsing adapter-specific list structures such as `streams[]` natively.

### Run bundled AF + adapter

In bundled mode, the adapter starts automatically inside `af`.

```bash
./build-demo-qod-bundle/bin/af --config config/af.yaml
```

By default the bundled adapter reads its own config from:

- `/etc/oai/af/demo_qod_adapter.yaml`, or
- the path in `PHINE_DEMO_QOD_ADAPTER_CONFIG`

It also supports a future `demo_qod_adapter:` section inside `af.yaml`.

### Docker

```bash
# From repo root
docker build -f adapters/demo-qod-adapter/Dockerfile -t demo-qod-adapter .
docker run --network phine-af-net demo-qod-adapter
```

### Docker (bundled AF + adapter)

```bash
# From repo root
docker build -f adapters/demo-qod-adapter/Dockerfile.bundle -t phine-af-demo-qod .
docker run --rm -p 50051:50051 phine-af-demo-qod
```

Or use the unified Compose file with the `demo-qod` profile:

```bash
docker compose -f docker-compose/compose.yaml --profile free5gc --profile demo-qod up --build
```

The bundled image copies:

- `config/af.yaml` to `/etc/oai/af/af.yaml`
- `adapters/demo-qod-adapter/config.yaml` to `/etc/oai/af/demo_qod_adapter.yaml`

The documented Compose entrypoint is:

- `docker-compose/compose.yaml`

Or use the standalone adapter together with split AF:

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

For bundled deployments, the same adapter configuration is read from `demo_qod_adapter.yaml`.

### Configuration loading model

The adapter uses two configuration paths:

- **Shared AF config handling** via `af::config::Configuration`
- **Native adapter parsing** for sequence-heavy adapter-specific data

Shared scalar values currently include:

- `af_core.address`
- `af_core.timeout_seconds`
- `retry.max_retries`
- `retry.initial_delay_ms`
- `monitor.interval_seconds`
- `monitor.iterations`

Adapter-native parsing is still used for:

- `streams[]`
- nested device port lists and ranges

This keeps the adapter aligned with the common configuration model without forcing all adapter-specific domain structures into `common/config`.

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

In bundled mode, the adapter still uses the existing gRPC client path and connects to the AF endpoint exposed by the bundled `af` process.

## Project Structure

```
demo-qod-adapter/
├── CMakeLists.txt
├── BundleInject.cmake
├── Dockerfile
├── Dockerfile.bundle
├── config.yaml
├── DESIGN.md
├── README.md
├── include/
│   ├── adapter_config.hpp   # shared adapter runtime config loader
│   ├── qod_client.hpp       # gRPC client (IQodClient interface + QodClient impl)
│   ├── qod_models.hpp       # Data models (Device, SessionInfo, Result<T>, etc.)
│   └── session_manager.hpp  # Session lifecycle manager
├── src/
│   ├── adapter_config.cpp
│   ├── bundled_entry.cpp
│   ├── main.cpp
│   ├── qod_client.cpp
│   └── session_manager.cpp
└── tests/
    ├── test_qod_client.cpp
    └── test_session_manager.cpp
```
