# 5G Application Function (AF) Microservice

## Overview

This repository contains a C++ microservice-based Application Function (AF) for 5G Core Networks. The AF serves as a crucial intermediary, connecting third-party applications and services with the capabilities of the 5G network, enabling enhanced functionalities such as dynamic Quality of Service (QoS) configuration, service function chaining, and data retrieval.

## Architecture

The AF is designed as a modular microservice application with distinct components responsible for specific functionalities:

1. **Northbound Interface Adapters**: Connect with external applications (ROS2, SFC MANO, etc.)
2. **AF Core Logic**: Orchestrates the overall functionality and routes requests
3. **Southbound Interface Handlers**: Communicate with 5G Core Network Functions (PCF, NEF, etc.)
4. **Data Management Component**: Manages application data, configurations, and state

### Architecture diagram

This diagram provides a high-level overview of the components and their relationships within your C++ microservice AF. Each box representing a microservice within the AF could potentially be a separate container in a deployment.

```mermaid
graph LR
    subgraph Third-Party Applications
        direction TB
        ROS2_App[ROS2 Applications / Robotics]
        SFC_MANO[SFC MANO System]
        Other_Apps[Other 3rd Party Applications]
    end

    subgraph 5G_Application_Function_Microservices [5G Application Function AF - Microservices]
        direction TB

        subgraph Northbound_Interfaces [Northbound Interface Adapters]
            direction TB
            ROS2_Service_Component[ROS2 Service Component]
            SFC_MANO_Interface[SFC MANO Interface Component]
            Generic_API_Component[Generic Application API Component]
        end

        AF_Core_Logic[AF Core Logic Component]

        subgraph Southbound_Interfaces [Southbound Interface Handlers]
            direction LR
            N5_Handler[N5 Handler]
            N33_Handler[N33 Handler]
            N52_Handler[N52 Handler]
            NX_BSF_Handler[NX-BSF Handler]
            NX_NWDAF_Handler[NX-NWDAF Handler]
            SMF_Interaction_Handler[SMF Interaction Handler]
            PIN_AF_Handler[PIN AF Handler]
            Naf_ProSe_Handler[Naf_ProSe Handler]
        end

        Data_Management[Data Management Component]

        %% Internal AF Connections
        ROS2_Service_Component --> AF_Core_Logic
        SFC_MANO_Interface --> AF_Core_Logic
        Generic_API_Component --> AF_Core_Logic
        AF_Core_Logic --> N5_Handler
        AF_Core_Logic --> N33_Handler
        AF_Core_Logic --> N52_Handler
        AF_Core_Logic --> NX_BSF_Handler
        AF_Core_Logic --> NX_NWDAF_Handler
        AF_Core_Logic --> SMF_Interaction_Handler
        AF_Core_Logic --> PIN_AF_Handler
        AF_Core_Logic --> Naf_ProSe_Handler
        AF_Core_Logic --> Data_Management
        Data_Management --> AF_Core_Logic
    end

    subgraph 5G_Core_Network_Functions [5G Core Network Functions NFs]
        direction TB
        PCF[Policy Control Function]
        NEF[Network Exposure Function]
        SMF[Session Management Function]
        TSCTSF[Time Sensitive Communication & Time Synchronization Function]
        BSF[Binding Support Function]
        NWDAF[Network Data Analytics Function]
        UDM[Unified Data Management]
        Other_NFs[Other NFs]
    end

    subgraph External_Entities [External Entities / Systems]
        direction TB
        PINEs[Public Interest Network Entities]
        ProSe_UEs[ProSe-enabled UEs]
    end

    %% Northbound Connections (Applications to AF)
    ROS2_App -- ROS2 Protocols --> ROS2_Service_Component
    SFC_MANO -- App-specific API (e.g., REST) --> SFC_MANO_Interface
    Other_Apps -- App-specific API (e.g., REST) --> Generic_API_Component

    %% Southbound Connections (AF to 5G Core NFs)
    N5_Handler -- N5 Interface --> PCF
    N33_Handler -- N33 Interface --> NEF
    N52_Handler -- N52 Interface (direct or via NEF) --> TSCTSF
    NEF --- TSCTSF
    NX_BSF_Handler -- Nbsf_Management Interface --> BSF
    NX_NWDAF_Handler -- Nnwdaf_EventsSubscription / Nnwdaf_AnalyticsInfo (via NEF or direct) --> NWDAF
    NEF --- NWDAF
    SMF_Interaction_Handler -- Sd-Nsmf / Nsmf_PDUSession (direct or via NEF) --> SMF
    NEF --- SMF
    PIN_AF_Handler -- PIN-DN communication (potentially via NEF) --> PINEs
    PIN_AF_Handler -- (via NEF) --> NEF
    Naf_ProSe_Handler -- Naf_ProSe Service --> ProSe_UEs

    %% NEF interactions with other NFs (for data retrieval/exposure)
    NEF -- Nudm / Nnrf etc. --> UDM
    NEF -- other NF interfaces --> Other_NFs


    classDef afMicroservice fill:#ccf,stroke:#333,stroke-width:2px;
    classDef extApp fill:#cfc,stroke:#333,stroke-width:2px;
    classDef coreNF fill:#fcc,stroke:#333,stroke-width:2px;
    classDef extEntity fill:#ffc,stroke:#333,stroke-width:2px;

    class ROS2_App,SFC_MANO,Other_Apps extApp;
    class 5G_Application_Function_Microservices,Northbound_Interfaces,AF_Core_Logic,Southbound_Interfaces,Data_Management afMicroservice;
    class ROS2_Service_Component,SFC_MANO_Interface,Generic_API_Component afMicroservice;
    class N5_Handler,N33_Handler,N52_Handler,NX_BSF_Handler,NX_NWDAF_Handler,SMF_Interaction_Handler,PIN_AF_Handler,Naf_ProSe_Handler afMicroservice;
    class PCF,NEF,SMF,TSCTSF,BSF,NWDAF,UDM,Other_NFs coreNF;
    class PINEs,ProSe_UEs extEntity;
```

## Project Structure

```
/oai-cn5g-af/
|-- common/               # Shared utilities, models, and frameworks
|-- northbound/           # Northbound interface adapters
|-- af_core/              # Core orchestration logic
|-- southbound/           # 5G Core Network interface handlers
|-- data_management/      # Data and state management
|-- build/                # Build scripts and configuration
|-- tests/                # Test suites
|-- docs/                 # Documentation
|-- config/               # Configuration files
```



## Prerequisites

- C++17 compatible compiler (GCC 8+, Clang 7+)
- CMake 3.14+
- Protocol Buffers (protobuf) 3.6+
- gRPC 1.16+ (optional, for gRPC communication)
- Bash (for build scripts)

For development:
- Git
- A modern IDE (Visual Studio Code, CLion, etc.)

## Building the Application

### Using docker compose

```bash
# Start setup
docker compose -f docker-compose/docker-compose-oai-build.yaml up -d


```

### Using the Build Script

The easiest way to build the application is to use the provided build script:

```bash
# Navigate to the project root
cd oai-cn5g-af

# Make the build script executable if needed
chmod +x build/scripts/build_all.sh

# Build the application (Release mode)
./build/scripts/build_all.sh

# Or with options
./build/scripts/build_all.sh --clean --deps
```

### Build Options

The build script supports several options:

- `--clean`: Clean the build directory before building
- `--deps`: Re/Install dependencies
- `--debug`: Build in debug mode (with debug symbols)
- `--tests`: Build and run tests
- `--install`: Install the built libraries and executables
- `--help`: Show help message

### Manual Build with CMake

If you prefer to use CMake directly:

```bash
# Navigate to the project root
cd oai-cn5g-afs

# Create and navigate to build directory
mkdir -p build-output
cd build-output

# Configure CMake
cmake -DCMAKE_BUILD_TYPE=Release ..

# Build
cmake --build . -- -j$(nproc)

# Optionally run tests
ctest

# Optionally install
cmake --install .
```

### Build Output

After a successful build:

- Compiled libraries will be in `build-output/lib/`
- Executables will be in `build-output/bin/`
- Generated protobuf/gRPC code will be in `build-output/generated/`

## Running the Application

*Note: This section will be expanded as the microservice components are implemented.*

## Testing Locally

### Quick Validation

Before pushing changes, run the local validation script to execute the same checks as CI/CD:

```bash
./.github/scripts/validate-locally.sh
```

This script will:
1. Check code quality (trailing whitespace, script permissions)
2. Build all components (AF Core, PCF Handler, API Component)
3. Build integration test suite
4. Run integration tests with Free5GC environment (~10-15 minutes)

### Viewing Test Results

Logs are saved to `/tmp/` for debugging:

```bash
# View build logs
tail -f /tmp/af-core-build.log
tail -f /tmp/pcf-handler-build.log

# View integration test logs
tail -f /tmp/integration-tests.log
```

### Running Specific Tests

```bash
cd docker-compose

# Run specific test suite
docker-compose -f docker-compose-test.yaml run --rm \
  -e GTEST_FILTER='QodIntegrationTest.*' \
  af_integration_tests

# Cleanup
docker-compose -f docker-compose-test.yaml down -v
```

## Development

### Code Structure

- **Common Code**: Shared utilities, models, and frameworks used across the application
- **Northbound Adapters**: Interface with third-party applications and translate their protocols
- **Core Logic**: Orchestrates request handling and manages AF state
- **Southbound Handlers**: Implement the 3GPP interfaces to 5G Core Network Functions
- **Data Management**: Handles data storage, caching, and retrieval

### Adding a New Component

1. Create a directory for your component
2. Add necessary source and header files
3. Create a CMakeLists.txt file for your component
4. Add your component to the root CMakeLists.txt
5. Update the build scripts if necessary

## Contributing

*Contribution guidelines will be added here*

## License

*License information will be added here*