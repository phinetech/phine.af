# High-Level Design Document

## 1. Introduction

This document provides the high-level architecture for Project AF, a C++ Application Function (AF) designed to expose 3GPP 5G network capabilities to third-party applications via the CAMARA API standard.

The primary goal of this project is to expose network capabilities to third-party applications through the CAMARA API standard. The design prioritizes modularity, scalability, and testability to ensure the system is robust, maintainable, and easy to extend with new services and connectors in the future.

This document is intended for developers, architects, and anyone interested in understanding the internal structure and design philosophy of the project.

## 2. Design Goals

The architecture is driven by the following key principles:

  * **Modularity:** Each component has a single, well-defined responsibility and can be developed, tested, and deployed independently.

  * **Clear Separation of Concerns:** A strict layering (Northbound, Core, Southbound) ensures that business logic is decoupled from communication protocols and external interfaces.

  * **Scalability:** The architecture supports both monolithic and microservice deployments, allowing the system to scale based on operational needs.

  * **Testability:** Components are loosely coupled using Dependency Injection and abstract interfaces, making them easy to mock and unit test.

  * **Extensibility:** The design makes it straightforward to add support for new CAMARA APIs or integrate with new 5G Network Functions (NFs).


## 3. High-Level Architecture

The system is divided into three primary logical layers: **Northbound**, **AF Core**, and **Southbound**. These layers communicate via a well-defined abstraction provided by the **Common** library.

### 3.1. Component Diagram

The following diagram illustrates the major components and the flow of information. An external application request comes in through a Northbound Gateway, is processed by the AF Core, which then uses a Southbound Connector to interact with the 5G Core Network. Note the central role of the Event Dispatcher within the AF Core for handling internal state propagation.

```mermaid
graph TD
    subgraph External World
        A[External Applications]
        N5G[5G Core Network]
    end

    subgraph Project AF
        NB(Northbound Gateway)
        
        subgraph AF Core
            direction LR
            subgraph State Managers
                UE_MGR(UeStateManager)
                QOD_MGR(QodSessionManager)
                LOC_MGR(...)
            end
            
            DISPATCHER(Event Dispatcher)

            UE_MGR -- Publishes Events --> DISPATCHER
            DISPATCHER -- Notifies --> QOD_MGR
            DISPATCHER -- Notifies --> LOC_MGR
        end

        SB(Southbound Connectors)
        
        subgraph Common Library
            COMMS[Communication Abstraction]
            DI[Dependency Injection]
            MODELS[Data Models]
        end
    end

    A --> NB
    NB -->|CAMARA API| AF Core
    AF Core -->|Internal Models| SB
    SB -->|3GPP Models| N5G
    N5G -->|Notifications| SB
    SB -->|Network Events| AF Core

```

### 3.2. Data Flow

1. An **External Application** sends a request to a **Northbound Gateway** using a specific protocol (e.g., OpenFlow).

2. The **Northbound Gateway** validates and maps this request into a standardized CAMARA API model.

3. The request is forwarded to the **AF Core**.

4. The **AF Core** applies the core business logic, manages state (e.g., creates a session), and translates the request into a generic, internal representation.

5. The **AF Core** routes the request to the appropriate **Southbound Connector** (e.g., `pcf_connector`).

6. The **Southbound Connector** maps the internal model to the specific 3GPP-compliant model required by the target Network Function (e.g., PCF) and communicates with the **5G Core Network**.

### 3.3. Internal AF Core Communication: The Event Bus

To avoid circular dependencies between different state managers (e.g., UeStateManager and QodSessionManager), the system uses a publish-subscribe model.

- Publishers: When a low-level state manager (like UeStateManager) detects a significant change (e.g., a PDU session is terminated), it publishes an event to the EventDispatcher. The publisher has no knowledge of who is listening.

- Subscribers: Higher-level, application-specific managers (like QodSessionManager) subscribe to the events they care about.

- Benefit: This inverts the dependency. Instead of the UeStateManager needing to know about every other manager, the other managers are responsible for listening for events that affect them. This makes the system highly extensible.

## 4. Component Deep Dive

### 4.1. Northbound Gateway

* **Responsibility:** Acts as a protocol adapter or translator. It exposes the AF's capabilities to external systems that do not speak the CAMARA API natively.

* **Key Files:** `protocol_manager.h`, `protocol_mapper.h`, `service.h`.

* **Behavior:** It understands a specific external protocol, converts its data format into a CAMARA model, and forwards it to the AF Core. Each gateway is self-contained and can be deployed independently.

### 4.2. AF Core

* **Responsibility:** The brain of the application. It is completely agnostic of the external protocols used by northbound clients and the specific 3GPP interfaces used by southbound connectors.

* **Key Features:**

  * **Service Logic:** Implements the business logic for each CAMARA API (e.g., `qod_service.h`).

  * **State Management:** Contains dedicated managers for different domains of state (`ue_state_manager.h`, `qod_state_manager.h`), with `ue_state_manager.h` being a central state manager.

  * **Orchestration:** Routes requests between northbound interfaces and the appropriate southbound connectors.

### 4.3. Southbound Connectors

* **Responsibility:** Manages communication with specific Network Functions (NFs) in the 5G Core. It isolates the AF Core from the complexities of 3GPP standards and protocols.

* **Key Features:**

  * **Protocol Handling:** Implements the client-side logic for interacting with an NF (e.g., `pcf_http2_client.h`).

  * **3GPP Mapping:** Maps internal data models to the exact 3GPP-specified models (`pcf_mapper.h`).

  * **Self-Contained:** Each connector contains all the logic needed to talk to a specific NF, allowing them to be developed and deployed independently.

### 4.4. Common Library

* **Responsibility:** Provides the foundational building blocks and shared code used across all other components. This library is crucial for enforcing consistency and reducing code duplication.

* **Key Modules:**

  * `communication/`: An abstraction layer that decouples components, allowing them to communicate via direct function calls (monolith) or gRPC (microservices) without changing their internal logic.

  * `di/`: A simple dependency injection framework to manage object lifecycles and promote loose coupling.

  * `models/`: Plain Old Data structures for the CAMARA APIs.

  * `state/`: Abstract base classes for state management patterns, defining a contract for the `af_core` to implement.

  * `utils/`: General-purpose utilities like logging and configuration management.

## 5. Detailed Project Structure

```
phine.af/
├── northbound/                     # Gateways for translating external protocols (e.g., OpenFlow, ROS2) into CAMARA API requests. Can be deployed independently.
|   ├── CMakeLists.txt              # Build script for the northbound gateway.
|   ├── Dockerfile                  # Defines the container for deploying this gateway as a standalone microservice.
│   ├── include/                    # Header files for the gateway logic.
|   |   ├── protocol_manager.h      # Handles the lifecycle and communication of the specific external protocol.
|   |   ├── protocol_mapper.h       # Maps data models from the external protocol to the standardized CAMARA models.
│   │   └──  service.h              # Orchestrates the gateway's logic, coordinating the manager and mapper to call the af_core.
|   └── src/                        # Source file implementations for the headers in include/.
│
├── af_core/                        # The core application. Contains all primary business logic for the CAMARA services.
|   ├── Dockerfile                  # Defines the container for deploying the core application.
│   ├── CMakeLists.txt              # Build script for the core application.
│   ├── include/                    # Header files for the core application logic.
│   │   ├── events/                 # Event definitions and dispatcher interface.
│   │   │   ├── events.h
│   │   │   ├── i_event_dispatcher.h
│   │   │   └── event_dispatcher.h  # Concrete implementation.
│   │   ├── state/                  # Manages the application's long-term state (e.g., active sessions, subscriptions).
│   │   │   ├── ue_state_manager.h  # Tracks and manages the state of individual User Equipments (UEs).
│   │   │   └── subscription_manager.h # Manages notification subscriptions for events.
│   │   │
│   │   └── services/               # Contains the business logic for each distinct CAMARA API service.
│   │       └── qod/                # Logic specific to the Quality on Demand (QoD) service.
│   │           ├── qod_service.h   # Main entry point and facade for the QoD service. It orchestrates all QoD operations.
|   |           ├── qod_session_manager.h # Handles the creation, modification, and deletion of QoD sessions.
|   |           ├── qod_state_manager.h # Manages state for CAMARA QualityOnDemand sessions
│   │           └── qod_api_mapper.h    # Maps incoming CAMARA API data models to the application's internal domain models.
│   │
│   └── src/                        # Source file implementations for the core application.
│       ├── state/                  # Implementations for state management logic.
│       │   ├── ue_state_manager.cpp
│       │   └── subscription_manager.cpp
│       └── services/
│           └── qod/                # Implementations for the QoD service logic.
│               ├── qod_service.cpp
│               ├── qod_session_manager.cpp
│               └── qod_api_mapper.cpp
│
├── southbound/                     # Connectors responsible for communicating with external network functions (NFs) in the 5G core.
│   ├── pcf_connector/              # Connector for the Policy Control Function (PCF).
|   |   ├── Dockerfile              # Defines the container for deploying the PCF connector as a standalone microservice.
│   │   ├── CMakeLists.txt          # Build script for the PCF connector.
│   │   ├── include/                # Header files for the PCF connector.
│   │   │   ├── pcf_service.h       # Main service entry point that receives requests from af_core and interacts with the PCF.
│   │   │   ├── pcf_mapper.h        # Maps the application's internal models to 3GPP-specific models required by the PCF.
│   │   │   ├── pcf_http2_client.h  # Low-level client for handling HTTP/2 communication with the PCF.
│   │   │   └── pcc_rule_factory.h  # Creates and validates Policy and Charging Control (PCC) rules for the PCF.
│   │   └── src/                    # Source file implementations for the PCF connector.
│   │
│   └── nef_connector/              # Connector for the Network Exposure Function (NEF).
|       ├── Dockerfile              # Defines the container for deploying the NEF connector as a standalone microservice.
│       ├── CMakeLists.txt          # Build script for the NEF connector.
│       ├── include/                # Header files for the NEF connector.
│       │   ├── nef_service.h       # Main service entry point that receives requests from af_core and interacts with the NEF.
│       │   ├── nef_mapper.h        # Maps the application's internal models to 3GPP-specific models required by the NEF.
│       │   └── nef_http_client.h   # Low-level client for handling HTTP communication with the NEF.
│       └── src/                    # Source file implementations for the NEF connector.
│
├── common/                         # A shared library containing code used across all other components (northbound, af_core, southbound).
│   ├── CMakeLists.txt              # Build script for the common shared library.
│   ├── communication/              # Abstraction layer for inter-service communication (e.g., gRPC vs. direct function calls).
|   |   ├── implementations/        # Concrete implementations of the communication interface.
|   |   |   ├── direct/             # In-process/direct function call implementation for monolithic deployments.
|   |   |   └── grpc/               # gRPC implementation for microservice deployments.
|   |   ├── include/                # Header files for the communication abstraction interfaces.
|   |   │   ├── communication_interface.h # Defines the abstract interface for all communication services.
|   |   │   └── request_router.h    # Routes incoming messages to the correct registered handler.
|   |   └── src/                    # Source file implementations for communication components.
|   ├── di/                         # Dependency Injection framework for managing object creation and wiring dependencies.
|   |   ├── include/
|   |   │   └── service_container.h # The main DI container that holds and resolves services.
|   |   └── src/
│   ├── models/                     # Contains Plain Old Data (POD) structures representing data models used throughout the application.
|   |   ├── qod/                    # Data models specific to the QoD API, often generated from OpenAPI/Swagger specifications.
|   |   |   ├── qod_session.h
|   |   |   └── qod_events.h
|   |   └── device_location/        # Data models for the Device Location API.
│   ├── state/                      # NEW: Abstract base classes for reusable state management patterns.
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   │   ├── base_ue_state_manager.h      # Defines the interface (e.g., abstract class) for a UE state manager.
│   │   │   └── base_subscription_manager.h  # Defines the interface for a subscription manager.
│   │   └── src/
│   │       ├── base_ue_state_manager.cpp      # Optional: Implementation for non-pure virtual functions in the base class.
│   │       └── base_subscription_manager.cpp
│   └── utils/                      # General-purpose utility functions (e.g., logging, configuration parsing).
|       ├── logger.h
|       └── config_manager.h
|
├── config/                         # Central location for all application configuration files.
│   ├── af_config.yaml              # Main application configuration (e.g., server ports, southbound endpoints).
│   └── logging.yaml                # Configuration for the logging framework (e.g., log levels, output files).
│
├── build/                          # Directory where build artifacts (executables, libraries) are placed. Not version controlled.
├── Dockerfile                      # A root Dockerfile, typically used for orchestrating a monolithic deployment of all components.
└── README.md                       # Project overview, setup instructions, and documentation.
```


### 6. Deployment Strategy

#### 6.1. Microservices Deployment

* Each component (`af_core`, `pcf_connector`, `northbound_gateway`) can be built and deployed in its own Docker container.

* The `grpc` implementation of the communication service is used for inter-service communication over the network.

* This model is ideal for scalability, resilience, and independent updates of components.

#### 6.2. Monolithic Deployment

* All components can be compiled into a single executable and run in one process.

* The `direct` implementation of the communication service is used, where inter-service communication becomes simple in-memory function calls.

* This model is simpler to deploy and debug, and can offer higher performance by avoiding network overhead.

## 7. Future Extensibility

The modular design makes it easy to extend the project's capabilities:

* **Adding a new CAMARA Service:** Create a new subdirectory in `af_core/services/` and `common/models/`, then implement the service logic.

* **Adding a new Southbound Connector:** Create a new subdirectory in `southbound/` containing the new service, mapper, and client for the target NF.

* **Adding a new Northbound Gateway:** Create a new subdirectory in `northbound/` to translate a new protocol.