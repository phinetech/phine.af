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
find_package(OpenSSL REQUIRED)

# Find nlohmann_json - use system installation from Debian package
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
    if(NLOHMANN_JSON_INCLUDE_DIR)
        # Create imported target for nlohmann_json (header-only)
        add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
        set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_INCLUDE_DIR}"
        )
        message(STATUS "Found nlohmann_json: ${NLOHMANN_JSON_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "nlohmann_json not found. Please install nlohmann-json3-dev")
    endif()
else()
    message(STATUS "Using existing nlohmann_json target")
endif()

# Find spdlog - use system installation from Debian package
if(NOT TARGET spdlog::spdlog)
    find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h)
    find_library(SPDLOG_LIBRARY NAMES spdlog)

    if(SPDLOG_INCLUDE_DIR AND SPDLOG_LIBRARY)
        # Create imported target for spdlog
        add_library(spdlog::spdlog UNKNOWN IMPORTED)
        set_target_properties(spdlog::spdlog PROPERTIES
            IMPORTED_LOCATION "${SPDLOG_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SPDLOG_INCLUDE_DIR}"
        )
        set(SPDLOG_TARGET spdlog::spdlog)
        message(STATUS "Found spdlog: ${SPDLOG_LIBRARY}")
        message(STATUS "spdlog include: ${SPDLOG_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "spdlog not found. Please install libspdlog-dev")
    endif()
else()
    set(SPDLOG_TARGET spdlog::spdlog)
    message(STATUS "Using existing spdlog target")
endif()

# Find yaml-cpp - use system installation from Debian package
if(NOT TARGET yaml-cpp)
    find_path(YAML_CPP_INCLUDE_DIR yaml-cpp/yaml.h)
    find_library(YAML_CPP_LIBRARY NAMES yaml-cpp)

    if(YAML_CPP_INCLUDE_DIR AND YAML_CPP_LIBRARY)
        # Create imported target for yaml-cpp
        add_library(yaml-cpp UNKNOWN IMPORTED)
        set_target_properties(yaml-cpp PROPERTIES
            IMPORTED_LOCATION "${YAML_CPP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${YAML_CPP_INCLUDE_DIR}"
        )
        message(STATUS "Found yaml-cpp: ${YAML_CPP_LIBRARY}")
        message(STATUS "yaml-cpp include: ${YAML_CPP_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "yaml-cpp not found. Please install libyaml-cpp-dev")
    endif()
else()
    message(STATUS "Using existing yaml-cpp target")
endif()

# Find fmt library (required by spdlog)
find_package(fmt REQUIRED)

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
    src/models/ue_key.cpp
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
            fmt::fmt
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
            fmt::fmt
            OpenSSL::SSL
            OpenSSL::Crypto
    )
endif()