# Communication Agnosticism & Dependency Injection

This document details the architectural patterns used in the project to achieve modularity and testability: Protocol Agnosticism via the Communication Framework and loose coupling via Dependency Injection.

## Communication Framework

The `common` library provides a text-book implementation of the Strategy pattern to decouple business logic from the underlying communication mechanism.

### The Communication Interface

At the core is the `af::communication::CommunicationService` abstract interface. Components interact exclusively with this interface to send requests, publish events, or subscribe to updates. They are unaware of whether the message travels over a network socket or a direct function call.

**Key capabilities:**
- **Synchronous Requests**: Request/Response pattern.
- **Asynchronous Messages**: Fire-and-forget or callback-based delivery.
- **Pub/Sub**: components can subscribe to topics.

### Implementations

The system currently supports two modes, switched via configuration:

1.  **gRPC (`grpc`)**: Used for microservice deployments. Components run in separate processes (or containers) and communicate over the network.
2.  **Direct (`direct`)**: Used for monolithic deployments or testing. Components run in the same process, and "messages" are essentially direct function calls, avoiding network overhead.

### The Factory

The `af::communication::CommunicationFactory` is responsible for instantiating the correct `CommunicationService` implementation at runtime based on the `target_service` configuration.

```cpp
// Example: Creating a communication service
auto comm_service = af::communication::CommunicationFactory::create_service(
    "grpc",             // type: "grpc" or "direct"
    "my_component",     // source service name
    config_map          // configuration
);
```

<!---
TODO: Add detailed guide on how to implement a new communication backend (e.g. MQTT or NATS).
-->

## Dependency Injection (DI)

To further decouple components and facilitate unit testing, the project implements a custom lightweight Dependency Injection container.

### Core Concepts

*   **ServiceContainer**: The central registry where services and their lifecycles (Singleton vs. Transient) are defined.
*   **ServiceProvider**: An interface passed to consumers to resolve dependencies without exposing the registration mechanism.

### Usage

**Registration (usually in `main.cpp` or a wiring helper):**
```cpp
ServiceContainer container;

// Register a Singleton
container.register_singleton<ILogger>([](ServiceProvider& p) {
    return std::make_shared<ConsoleLogger>();
});

// Register a Factory (New instance every time)
container.register_factory<IRequestHandler>([](ServiceProvider& p) {
    auto deps = p.get<ILogger>();
    return std::make_shared<MyRequestHandler>(deps);
});
```

**Resolution:**
```cpp
// In your component
auto logger = provider.get<ILogger>();
```

### Benefits for Testing

The DI system makes it trivial to swap real implementations with mocks during testing.

```cpp
// In a test setup
container.register_singleton<ICommunicationService>([](ServiceProvider& p) {
    return std::make_shared<MockCommunicationService>();
});
```

<!---
TODO: Add a complete example of wiring up a new module with DI, including configuration injection.
-->
