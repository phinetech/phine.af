# Repository layout

## Top-level directories

- `af_core/`: Core orchestration logic and CAMARA service logic
- `northbound/`: Gateways/adapters that translate external protocols to internal/CAMARA models
- `southbound/`: NF-specific connectors/handlers (e.g., PCF/NEF)
- `common/`: Shared libraries (communication abstraction, models, DI, utilities)
- `docker-compose/`: Compose definitions for build and test environments
- `build/`: Build scripts

<!---
TODO: Add “where to start” pointers for new developers (key entry points, most important CMakeLists.txt, etc.) after a quick codebase walkthrough.
-->
