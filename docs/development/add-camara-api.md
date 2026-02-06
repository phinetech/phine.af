# Add a new CAMARA API

This page documents the *repository-derived* steps that appear to be required to add a new CAMARA API.

## High-level steps

1. Create a new service directory under `af_core` (see `af_core` QoD as an example).
2. Add/extend data models under `common/models`.
3. Register message routing for the new API in AF Core.

## AF Core wiring checklist

- Add message types to the main communication service handler registration.
- Register the message type in the request router to call your service handler.

<!---
TODO: Provide a concrete, step-by-step “copy QoD and adapt” tutorial once we inspect the QoD directory structure and its CMake integration.
-->
