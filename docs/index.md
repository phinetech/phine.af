# phine.af Documentation

This repository contains a C++ Application Function (AF) for 5G Core Networks.

The project is designed to expose 3GPP 5G network capabilities to third-party applications via the CAMARA API standard, using a modular architecture intended to be scalable, testable, and extensible.

📖 Hosted at <https://af.phine.tech/>. Source code:
<https://github.com/phinetech/phine.af>.

## What to read next


- Start here: [Getting Started](getting-started/05-quickstart.md)
- Full QoS enforcement walkthrough: [QoS Enforcement Tutorial](getting-started/06-qos-http-tutorial.md)
- Run the QoD adapter demo: [Adapter Tutorial](getting-started/08-demo-adapter-tutorial.md)
- Understand the design: [Architecture Overview](architecture/overview.md)
- See what is implemented: [Status](status.md)
- Developer workflows: [Development](development/add-camara-api.md)
- Contribute: [Contributing](contributing.md)

## Repository layout


- `common/`: Shared utilities, models, and frameworks
- `northbound/`: Northbound APIs (e.g. the HTTP/2 REST `api_component` exposing CAMARA endpoints)
- `af_core/`: Core orchestration logic
- `southbound/`: 5G Core Network interface handlers (e.g. `pcf_handler`)
- `adapters/`: Example application-side adapters (e.g. [phine.af-demo-qod-adapter](../adapters/demo-qod-adapter/README.md) — an application-management demo for ROS2-over-5G)
- `docker-compose/`: Docker Compose environments (free5gc, OAI CN5G, UERANSIM)
- `build/`: Build scripts and configuration
- `tests/`: Test suites