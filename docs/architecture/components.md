# Components

## AF Core

### AF Orchestrator
- Central coordinator for AF functionality
- Initialises and manages other components
- Establishes communication channels
- Routes incoming messages to handlers

### Request Router
- Routes messages to handlers based on message type

### Policy Manager
- Manages QoS policies
- Interfaces with PCF to apply policies

### Subscription Manager
- Handles subscription lifecycle and event delivery

## QoD (Quality on Demand)

- AF Core registers QoD message types including: `qod_create_session`, `qod_get_session`, `qod_delete_session`, `qod_extend_session`, `qod_retrieve_sessions`.

<!---
TODO: Add detailed QoD service internals once we inspect the QoD handler/session manager implementations under `af_core`.
-->
