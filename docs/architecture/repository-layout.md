# Repository layout

## Top-level directories

- `af_core/`: Core orchestration logic and CAMARA service logic. Entry
  point: [`af_core/src/main.cpp`](../../af_core/src/main.cpp); CMake target:
  `af_core`.
- `northbound/`: Northbound APIs. Currently ships `northbound/api_component/`,
  an HTTP/2 REST service (nghttp2 + boost::asio) exposing CAMARA endpoints
  (`POST /qos`, `/subscriptions`, `/health`, …) that forwards to `af_core`
  over the configured communication backend. CMake target: `api_server`.
- `southbound/`: NF-specific connectors/handlers. Currently ships
  `southbound/pcf_handler/` (N5 / `Npcf_PolicyAuthorization`). CMake target:
  `pcf_server`. The `southbound/oai-cn5g-common-src/` folder is an upstream
  OAI CN5G submodule providing shared C++ CN5G libraries.
- `common/`: Shared libraries (communication abstraction, config loader, DI
  container, models, utilities). Consumed by every component.
- `adapters/`: Example application-side adapters — external apps that
  *consume* phine.af's northbound APIs. `adapters/demo-qod-adapter/`
  simulates the Application Management component for a ROS2-over-5G robot
  requesting QoD sessions.
- `docker-compose/`: Compose definitions for build and test environments
  (free5gc, OAI CN5G, UERANSIM).
- `build/`: Build scripts, CMake helpers, diagram sync tool.
- `tests/`: Cross-component test suites (each component also has its own
  `tests/`).
- `docs/`: This documentation site.
- `config/`: Sample deployment configuration files.

## Where to start

- New contributor build/run flow: [Getting Started](../getting-started/05-quickstart.md).
- Root CMake: [`CMakeLists.txt`](../../CMakeLists.txt) — controls both the
  microservice build and the bundled `af` executable (`-DBUILD_BUNDLED=ON`).
- Communication abstraction: [`common/communication/`](../../common/communication)
  and the [Communication & DI guide](communication-and-di.md).