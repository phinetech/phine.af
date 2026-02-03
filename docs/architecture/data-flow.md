# Data flow


## Request path (northbound → core → southbound)

1. An external application sends a request to a Northbound Gateway (following a particular CAMARA API model).
2. The Northbound Gateway receives the request in CAMARA API model and forwards it to AF Core.
3. AF Core applies business logic and routes the request to the relavant Southbound Connector(s).
4. The Southbound Connector maps internal models to 3GPP models and communicates with the target Network Function.

## Internal eventing

The design uses a publish-subscribe model (Event Dispatcher) to avoid circular dependencies between state managers.

<!---
TODO: Add a concrete example of an event type and subscribers once we confirm which events are actively used.
Note: There is a commented-out subscription to `PduSessionTerminatedEvent` in [af_core/src/af_orchestrator.cpp](../../af_core/src/af_orchestrator.cpp).
-->
