# Northbound APIs

## Northbound API Component


The `northbound/api_component` provides an HTTP/2 REST API for third-party applications to interact with the AF.

It mentions JSON-based APIs for QoS management and event subscriptions, and uses YAML for configuration.

### Implemented Endpoints

The following HTTP/2 endpoints are currently implemented in the `api_adapter`:

| Method | Endpoint | Description |
|---|---|---|
| `GET` | `/health` | Health check endpoint. Returns status "up". |
| `GET` | `/version` | Returns component version and build information. |
| `POST` | `/qos` | Request Quality of Service adjustment. |
| `GET` | `/subscriptions` | Retrieve a list of active subscriptions. |
| `POST` | `/subscriptions` | Create a new subscription. |
| `GET` | `/subscriptions/{id}` | Retrieve details of a specific subscription. |
| `DELETE` | `/subscriptions/{id}` | Delete a specific subscription. |


## AF Core message types

The AF Core registers handlers for (non-exhaustive):

- `health_check`
- `qos_request`
- `get_subscriptions`, `create_subscription`, `get_subscription`, `delete_subscription`
- `qod_create_session`, `qod_get_session`, `qod_delete_session`, `qod_extend_session`, `qod_retrieve_sessions`

<!---
TODO: Map northbound HTTP endpoints to these message types.
The API component README includes sequence diagrams but does not list the complete HTTP surface.
-->
