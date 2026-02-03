# phine.af Documentation

This repository contains a C++ Application Function (AF) for 5G Core Networks.

The project is designed to expose 3GPP 5G network capabilities to third-party applications via the CAMARA API standard, using a modular architecture intended to be scalable, testable, and extensible.

## What to read next


- Start here: [Getting Started](getting-started/quickstart.md)
- Understand the design: [Architecture Overview](architecture/overview.md)
- See what is implemented: [Status](status.md)
- Developer workflows: [Development](development/add-camara-api.md)

## Repository layout


- `common/`: Shared utilities, models, and frameworks
- `northbound/`: Northbound interface adapters
- `af_core/`: Core orchestration logic
- `southbound/`: 5G Core Network interface handlers
- `docker-compose/`: Docker Compose environments
- `build/`: Build scripts and configuration
- `tests/`: Test suites

<!---
TODO: Add links to the most important subdirectories once we confirm which are stable entry points for contributors.
No explicit "docs entry point" guidance found in the repo.
-->
