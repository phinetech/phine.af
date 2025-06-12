

## AF Components 

The AF Core consists of the following main components:

### AF Orchestrator
- **Central coordinator** for all AF functionality
- Initializes and manages all other components
- Establishes communication channels with northbound and southbound interfaces
- Routes incoming messages to appropriate handlers

### Request Router
- Routes messages to appropriate handlers based on message type
- Provides a clean separation between message reception and processing
- Supports default handling for unknown message types

### Policy Manager
- Manages QoS policies and related network configurations
- Validates policy requests from applications
- Translates application requirements to network policies
- Interfaces with PCF to apply policies to the 5G network
- Maintains state of active policies

### Subscription Manager
- Handles event subscriptions from applications
- Manages subscription lifecycle (creation, retrieval, deletion)
- Delivers notifications when relevant events occur
- Supports both webhook-based and service-to-service notifications
- Handles filtering and subscription expiration

```mermaid
graph TB
    subgraph "Northbound Interfaces"
        api[API Adapter]
        ros2[ROS2 Adapter]
        sfc[SFC MANO Adapter]
    end

    subgraph "AF Core"
        orch[AF Orchestrator]
        router[Request Router]
        policy[Policy Manager]
        sub[Subscription Manager]
        
        orch --> router
        orch --> policy
        orch --> sub
        
        router --> policy
        router --> sub
    end

    subgraph "Southbound Interfaces"
        pcf[PCF Handler]
        nef[NEF Handler]
        tsctsf[TSCTSF Handler]
        bsf[BSF Handler]
        nwdaf[NWDAF Handler]
    end
    
    subgraph "5G Core Network"
        core_pcf[PCF]
        core_nef[NEF]
        core_tsctsf[TSCTSF]
        core_bsf[BSF]
        core_nwdaf[NWDAF]
    end
    
    %% Northbound connections
    api -- "gRPC" --> orch
    ros2 -- "gRPC" --> orch
    sfc -- "gRPC" --> orch
    
    %% Southbound connections
    orch -- "gRPC" --> pcf
    orch -- "gRPC" --> nef
    orch -- "gRPC" --> tsctsf
    orch -- "gRPC" --> bsf
    orch -- "gRPC" --> nwdaf
    
    %% 5G Core connections
    pcf -- "HTTP/2 (N5)" --> core_pcf
    nef -- "HTTP/2 (N33)" --> core_nef
    tsctsf -- "HTTP/2 (N52)" --> core_tsctsf
    bsf -- "HTTP/2" --> core_bsf
    nwdaf -- "HTTP/2" --> core_nwdaf
    
    %% Data Store
    db[(Data Storage)]
    orch -- "R/W" --> db
    
    classDef core fill:#f9f,stroke:#333,stroke-width:2px
    classDef northbound fill:#bbf,stroke:#333,stroke-width:2px
    classDef southbound fill:#bfb,stroke:#333,stroke-width:2px
    classDef external fill:#fbb,stroke:#333,stroke-width:2px
    classDef storage fill:#bff,stroke:#333,stroke-width:2px
    
    class orch,router,policy,sub core
    class api,ros2,sfc northbound
    class pcf,nef,tsctsf,bsf,nwdaf southbound
    class core_pcf,core_nef,core_tsctsf,core_bsf,core_nwdaf external
    class db storage
```

## Sequence diagram - QoS Policy Creation Flow

```mermaid
sequenceDiagram
    participant Client as API Client
    participant API as API Adapter
    participant Orch as AF Orchestrator
    participant Router as Request Router
    participant Policy as Policy Manager
    participant PCF as PCF Handler
    participant CorePCF as 5G Core PCF
    
    Client->>API: HTTP POST /qos
    API->>Orch: send_request("qos_request")
    Orch->>Router: route_message(msg)
    Router->>Policy: handle_qos_request(msg)
    
    Policy->>Policy: validate_policy()
    
    Policy->>PCF: send_request("pcf_policy_create")
    PCF->>CorePCF: HTTP POST /npcf-policyauthorization/v1/app-sessions
    CorePCF-->>PCF: 201 Created (Policy ID)
    PCF-->>Policy: policy_created response
    
    Policy->>Policy: store policy
    Policy-->>Router: success response
    Router-->>Orch: success response
    Orch-->>API: success response
    API-->>Client: HTTP 200 OK (Policy ID)
```

## Sequence diagram - Subscription Management Flow

```mermaid
sequenceDiagram
    participant Client as API Client
    participant API as API Adapter
    participant Orch as AF Orchestrator
    participant Router as Request Router
    participant SubMgr as Subscription Manager
    
    Client->>API: HTTP POST /subscriptions
    API->>Orch: send_request("create_subscription")
    Orch->>Router: route_message(msg)
    Router->>SubMgr: create_subscription(msg)
    
    SubMgr->>SubMgr: validate_subscription()
    SubMgr->>SubMgr: generate_subscription_id()
    SubMgr->>SubMgr: store subscription
    
    SubMgr-->>Router: subscription_created response
    Router-->>Orch: subscription_created response
    Orch-->>API: subscription_created response
    API-->>Client: HTTP 201 Created (Subscription ID)
    
    Note over Client,SubMgr: Later: Get Subscription
    
    Client->>API: HTTP GET /subscriptions/{id}
    API->>Orch: send_request("get_subscription")
    Orch->>Router: route_message(msg)
    Router->>SubMgr: get_subscription(msg)
    SubMgr-->>Router: subscription details
    Router-->>Orch: subscription details
    Orch-->>API: subscription details
    API-->>Client: HTTP 200 OK (Subscription Details)
```

## Sequence diagram - Event Notification Flow

```mermaid
sequenceDiagram
    participant PCF as PCF Handler
    participant CorePCF as 5G Core PCF
    participant Orch as AF Orchestrator
    participant SubMgr as Subscription Manager
    participant API as API Adapter
    participant Client as API Client
    
    CorePCF->>PCF: HTTP POST /notification (network event)
    PCF->>Orch: send_request("network_event")
    
    Orch->>SubMgr: notify_event("network_change", data)
    
    SubMgr->>SubMgr: Find matching subscriptions
    
    loop For each matching subscription
        alt External notification (webhook)
            SubMgr->>Client: HTTP POST to notification_url
        else Internal notification (service)
            SubMgr->>API: send_async("event_notification")
            API->>Client: Forward notification
        end
    end
    
    SubMgr-->>Orch: Notification count
    Orch-->>PCF: Success response
    PCF-->>CorePCF: HTTP 200 OK
```


## Key Message Types

| Message Type | Direction | Description |
|--------------|-----------|-------------|
| `qos_request` | Northbound→Core | Request to create/update QoS policy |
| `qos_success` | Core→Northbound | Successful QoS policy operation |
| `qos_error` | Core→Northbound | Error in QoS policy operation |
| `create_subscription` | Northbound→Core | Create new event subscription |
| `subscription_created` | Core→Northbound | Subscription successfully created |
| `get_subscriptions` | Northbound→Core | Request list of subscriptions |
| `subscriptions_list` | Core→Northbound | List of subscriptions |
| `get_subscription` | Northbound→Core | Request specific subscription |
| `subscription` | Core→Northbound | Subscription details |
| `delete_subscription` | Northbound→Core | Delete a subscription |
| `subscription_deleted` | Core→Northbound | Subscription successfully deleted |
| `event_notification` | Core→Northbound | Event notification to subscriber |
| `pcf_create_app_session` | Core→Southbound | Create policy in PCF |
| `pcf_update_app_session` | Core→Southbound | Update policy in PCF |
| `pcf_delete_app_session` | Core→Southbound | Delete policy in PCF |
| `network_event` | Southbound→Core | Network event notification |

This architecture enables the AF to effectively bridge between applications and the 5G core network, providing dynamic QoS management, network event notifications, and other advanced features.

## Testing Northbound Endpoints

We will use grpcurl running inside a docker container to simplify setup. Assuming the IP address for the af_core is `192.168.73.131`. The UERANSIM in the docker compose sets up a UE that has a static UE IP of 12.1.1.10.

```bash
docker run --rm --network host -v ./common/protos:/var/protos/ fullstorydev/grpcurl -plaintext \
  -proto message.proto \
  -import-path /var/protos \
  -d '{
    "message_type": "qos_request",
    "correlation_id": "12345",
    "payload": "'$(echo '{"ue_ipv4":"12.1.1.4"}' | base64)'",
    "metadata": {
      "source": "command_line",
      "priority": "high"
    }
  }' \
  192.168.70.141:50051 \
  af.proto.InternalCommunication/SendMessage
```