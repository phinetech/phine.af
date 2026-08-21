# phine.af Documentation

This repository contains a C++ Application Function (AF) for 5G Core Networks.

The project is designed to expose 3GPP 5G network capabilities to third-party applications via the CAMARA API standard, using a modular architecture intended to be scalable, testable, and extensible.

## What to read next


- Start here: [Getting Started](getting-started/05-quickstart.md)
- Full QoS enforcement walkthrough: [QoS Enforcement Tutorial](getting-started/06-qos-http-tutorial.md)
- Run the QoD adapter demo: [Adapter Tutorial](getting-started/08-demo-adapter-tutorial.md)
- Understand the design: [Architecture Overview](architecture/overview.md)
- See what is implemented: [Status](status.md)
- Developer workflows: [Development](development/add-camara-api.md)

## Repository layout


- `common/`: Shared utilities, models, and frameworks
- `northbound/`: Northbound interface adapters
- `af_core/`: Core orchestration logic
- `southbound/`: 5G Core Network interface handlers
- `adapters/`: Demo adapter applications (e.g. [demo-qod-adapter](../adapters/demo-qod-adapter/README.md))
- `docker-compose/`: Docker Compose environments
- `build/`: Build scripts and configuration
- `tests/`: Test suites

<!---
TODO: Add links to the most important subdirectories once we confirm which are stable entry points for contributors.
No explicit "docs entry point" guidance found in the repo.
-->
