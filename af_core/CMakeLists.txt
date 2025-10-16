cmake_minimum_required(VERSION 3.14)
project(af_core VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find Protobuf FIRST (required by gRPC)
find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()

# Create aliases if they don't exist
if(NOT TARGET protobuf::libprotobuf)
    if(TARGET protobuf::protobuf)
        add_library(protobuf::libprotobuf ALIAS protobuf::protobuf)
    elseif(DEFINED Protobuf_LIBRARIES)
        add_library(protobuf::libprotobuf UNKNOWN IMPORTED)
        set_target_properties(protobuf::libprotobuf PROPERTIES
            IMPORTED_LOCATION "${Protobuf_LIBRARIES}"
            INTERFACE_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIRS}"
        )
    endif()
endif()

if(NOT TARGET protobuf::libprotoc)
    find_library(PROTOC_LIBRARY NAMES protoc)
    if(PROTOC_LIBRARY)
        add_library(protobuf::libprotoc UNKNOWN IMPORTED)
        set_target_properties(protobuf::libprotoc PROPERTIES
            IMPORTED_LOCATION "${PROTOC_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${Protobuf_INCLUDE_DIRS}"
        )
    endif()
endif()

if(NOT TARGET protobuf::protoc)
    if(TARGET protobuf::protoc-bin)
        add_executable(protobuf::protoc ALIAS protobuf::protoc-bin)
    elseif(DEFINED Protobuf_PROTOC_EXECUTABLE)
        # Create an imported executable target
        add_executable(protobuf::protoc IMPORTED)
        set_target_properties(protobuf::protoc PROPERTIES
            IMPORTED_LOCATION "${Protobuf_PROTOC_EXECUTABLE}"
        )
    endif()
endif()

message(STATUS "Protobuf found: ${Protobuf_VERSION}")
message(STATUS "Protobuf include: ${Protobuf_INCLUDE_DIRS}")

# Now find dependencies that depend on Protobuf
find_package(gRPC REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)

# Try to find the installed af_common library
find_library(AF_COMMON_LIB NAMES af_common libaf_common)
find_library(AF_PROTOS_LIB NAMES af_protos libaf_protos)
find_path(AF_COMMON_INCLUDE_DIR NAMES af/common PATHS /usr/local/include NO_DEFAULT_PATH)

if(AF_COMMON_LIB AND AF_COMMON_INCLUDE_DIR)
    message(STATUS "Found af_common library: ${AF_COMMON_LIB}")
    message(STATUS "Found af_protos library: ${AF_PROTOS_LIB}")
    message(STATUS "Found af_common includes: ${AF_COMMON_INCLUDE_DIR}")
    set(USE_INSTALLED_AF_COMMON TRUE)
else()
    message(STATUS "af_common not installed, adding as subdirectory")
    add_subdirectory(common)
    set(USE_INSTALLED_AF_COMMON FALSE)
endif()

# Define test sources
set(SOURCES
    src/qod/qod_notification_manager.cpp
    src/qod/qod_session_manager.cpp
    src/qod/qod_state_manager.cpp
    src/qod/qod_handler.cpp
    src/af_orchestrator.cpp
    src/request_router.cpp
    src/policy_manager.cpp
    src/subscription_manager.cpp
    src/ue_state_manager.cpp
    src/main.cpp
)

# Create test executable
add_executable(af_core ${SOURCES})

# Set include directories based on whether we're using installed library or not
if(USE_INSTALLED_AF_COMMON)
    target_include_directories(af_core
        PRIVATE
            include
            ${AF_COMMON_INCLUDE_DIR}
    )
else()
    target_include_directories(af_core
        PRIVATE
            include
    )
endif()

# Link libraries
if(USE_INSTALLED_AF_COMMON)
    target_link_libraries(af_core
        PRIVATE
            ${AF_COMMON_LIB}
            ${AF_PROTOS_LIB}
            gRPC::grpc++
            gRPC::grpc++_reflection
            protobuf::libprotobuf
            nlohmann_json::nlohmann_json
            OpenSSL::SSL
            OpenSSL::Crypto
    )
else()
    target_link_libraries(af_core
        PRIVATE
            af_common
            gRPC::grpc++
            gRPC::grpc++_reflection
            protobuf::libprotobuf
            nlohmann_json::nlohmann_json
            OpenSSL::SSL
            OpenSSL::Crypto
    )
endif()