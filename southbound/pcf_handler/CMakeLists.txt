# CMakeLists.txt for PCF Handler module
# This version uses pre-installed OAI CN5G Common libraries

cmake_minimum_required(VERSION 3.14)
project(pcf_handler VERSION 1.0.0 LANGUAGES CXX)

set(GIT_BRANCH        "UNKNOWN")
set(GIT_COMMIT_HASH   "UNKNOWN")
set(GIT_COMMIT_DATE   "UNKNOWN")
add_definitions("-DPACKAGE_VERSION=\"Branch: ${GIT_BRANCH} Abrev. Hash: ${GIT_COMMIT_HASH} Date: ${GIT_COMMIT_DATE}\"")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for tooling (e.g. clang-tidy)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add custom cmake module path
# In Docker builds, common is copied to /app/common/ while we're in /app/southbound/pcf_handler/
# Otherwise, it's at ../common (one level up in the southbound directory)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/common/cmake")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/common/cmake")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../common/cmake")
    # Docker build: /app/southbound/pcf_handler/ -> /app/common/
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../common/cmake")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../common/cmake")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../common/cmake")
endif()

# Use system libraries by default
option(USE_SYSTEM_OPENSSL "Use system installed OpenSSL" ON)
option(USE_SYSTEM_NGHTTP2 "Use system installed nghttp2" ON)
option(USE_SYSTEM_BOOST "Use system installed Boost" ON)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Find Protobuf FIRST (required by gRPC)
# Guard against re-importing when already defined by a parent CMake scope
if(NOT TARGET protobuf::libprotobuf)
    find_package(Protobuf CONFIG QUIET)
    if(NOT Protobuf_FOUND)
        find_package(Protobuf REQUIRED)
    endif()
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

# Find dependencies
find_package(gRPC REQUIRED)

# Find additional gRPC libraries manually
find_library(GPR_LIB NAMES gpr PATHS /usr/local/lib)
if(GPR_LIB AND NOT TARGET gRPC::gpr)
    add_library(gRPC::gpr STATIC IMPORTED)
    set_target_properties(gRPC::gpr PROPERTIES IMPORTED_LOCATION "${GPR_LIB}")
endif()

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

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

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

find_package(Boost REQUIRED COMPONENTS system thread)
find_package(NGHTTP2 REQUIRED)
find_package(NGHTTP2_ASIO REQUIRED)

# Find Abseil libraries required by gRPC
set(ABSL_LIBRARIES
    absl_synchronization
    absl_strings
    absl_base
    absl_time
    absl_int128
    absl_status
    absl_statusor
    absl_cord
    absl_raw_hash_set
    absl_hash
    absl_throw_delegate
)

foreach(ABSL_LIB ${ABSL_LIBRARIES})
    find_library(${ABSL_LIB}_LIBRARY NAMES ${ABSL_LIB} PATHS /usr/local/lib)
    if(${ABSL_LIB}_LIBRARY)
        message(STATUS "Found ${ABSL_LIB}: ${${ABSL_LIB}_LIBRARY}")
    else()
        message(WARNING "${ABSL_LIB} not found")
    endif()
endforeach()

# Set the list of Abseil libraries to link (with -l prefix for direct linking)
set(ABSL_LINK_LIBRARIES
    -labsl_synchronization
    -labsl_strings
    -labsl_base
    -labsl_time
    -labsl_int128
    -labsl_status
    -labsl_statusor
    -labsl_cord
    -labsl_raw_hash_set
    -labsl_hash
    -labsl_throw_delegate
    -labsl_strings_internal
    -labsl_graphcycles_internal
    -labsl_stacktrace
    -labsl_symbolize
    -labsl_malloc_internal
    -labsl_debugging_internal
    -labsl_demangle_internal
    -labsl_time_zone
    -labsl_raw_logging_internal
    -labsl_log_severity
    -labsl_spinlock_wait
)

# Find OAI CN5G Common libraries
find_package(OAI REQUIRED)
message(STATUS "Found OAI libraries: ${OAI_LIBRARIES}")
message(STATUS "OAI include dirs: ${OAI_INCLUDE_DIRS}")

# Use the root common library (built by parent CMakeLists.txt)
# No need to add_subdirectory or find installed library - parent handles it

# Define library sources (everything except main.cpp)
set(LIB_SOURCES
    src/helpers.cpp
    src/qod_pcf_handler.cpp
    src/request_router.cpp
    src/pcf_handler.cpp
    src/pcc_rule_manager.cpp
    src/http_pcf_gateway.cpp
)

# Create static library for reuse by bundled build
add_library(pcf_handler_lib STATIC ${LIB_SOURCES})

# Create standalone executable
add_executable(pcf_server src/main.cpp)

# Set include directories - add common library include paths
# For Docker builds, use installed paths; for source builds, use relative paths
set(COMMON_INCLUDE_DIRS "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../common")
    # Source build - common is at repo root (includes Docker build with /app/common)
    list(APPEND COMMON_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/component/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/communication/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/communication/implementations/http/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/models
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/models/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/config/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/di/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/cpp_utils/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/handlers/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/response/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../common/communication/adapters/include
    )
elseif(EXISTS "/usr/local/include/af")
    # Docker build - use installed headers (fallback)
    list(APPEND COMMON_INCLUDE_DIRS
        /usr/local/include/af
        /usr/local/include/af/common
    )
endif()

# Find AF common libraries
# Check if targets exist (parent build) or find installed libraries (standalone Docker build)
set(AF_COMMON_LIBS "")
if(TARGET af_common)
    list(APPEND AF_COMMON_LIBS af_common af_common_component af_communication af_grpc_communication af_http_communication af_protos)
else()
    # Find installed AF common libraries
    find_library(AF_COMMON_LIB NAMES af_common PATHS /usr/local/lib)
    find_library(AF_CONFIG_LIB NAMES af_config PATHS /usr/local/lib)
    find_library(AF_COMMON_COMPONENT_LIB NAMES af_common_component PATHS /usr/local/lib)
    find_library(AF_COMMUNICATION_LIB NAMES af_communication_factory af_communication PATHS /usr/local/lib)
    find_library(AF_GRPC_COMMUNICATION_LIB NAMES af_grpc_communication PATHS /usr/local/lib)
    find_library(AF_DIRECT_COMMUNICATION_LIB NAMES af_direct_communication PATHS /usr/local/lib)
    find_library(AF_PROTOS_LIB NAMES af_protos PATHS /usr/local/lib)
    find_library(AF_RESPONSE_LIB NAMES af_response PATHS /usr/local/lib)
    find_library(AF_TRANSPORT_ADAPTERS_LIB NAMES af_transport_adapters PATHS /usr/local/lib)
    find_library(AF_HTTP_COMMUNICATION_LIB NAMES af_http_communication PATHS /usr/local/lib)
    find_library(AF_HANDLERS_LIB NAMES af_handlers PATHS /usr/local/lib)

    if(AF_COMMON_LIB)
        list(APPEND AF_COMMON_LIBS ${AF_COMMON_LIB})
        message(STATUS "Found af_common: ${AF_COMMON_LIB}")
    endif()
    if(AF_CONFIG_LIB)
        list(APPEND AF_COMMON_LIBS ${AF_CONFIG_LIB})
        message(STATUS "Found af_config: ${AF_CONFIG_LIB}")
    endif()
    if(AF_COMMON_COMPONENT_LIB)
        list(APPEND AF_COMMON_LIBS ${AF_COMMON_COMPONENT_LIB})
        message(STATUS "Found af_common_component: ${AF_COMMON_COMPONENT_LIB}")
    endif()
    if(AF_COMMUNICATION_LIB)
        list(APPEND AF_COMMON_LIBS ${AF_COMMUNICATION_LIB})
        message(STATUS "Found af_communication: ${AF_COMMUNICATION_LIB}")
    endif()
    if(AF_GRPC_COMMUNICATION_LIB)
        list(APPEND AF_COMMON_LIBS ${AF_GRPC_COMMUNICATION_LIB})
        message(STATUS "Found af_grpc_communication: ${AF_GRPC_COMMUNICATION_LIB}")
    endif()
    if(AF_DIRECT_COMMUNICATION_LIB)
        list(APPEND AF_COMMON_LIBS ${AF_DIRECT_COMMUNICATION_LIB})
        message(STATUS "Found af_direct_communication: ${AF_DIRECT_COMMUNICATION_LIB}")
    endif()
    if(AF_PROTOS_LIB)
        list(APPEND AF_COMMON_LIBS ${AF_PROTOS_LIB})
        message(STATUS "Found af_protos: ${AF_PROTOS_LIB}")
    endif()
    # Link order matters for static libraries - dependencies must come after
    # af_handlers depends on af_response and af_transport_adapters
    if(AF_HANDLERS_LIB)
        list(APPEND AF_COMMON_LIBS ${AF_HANDLERS_LIB})
        message(STATUS "Found af_handlers: ${AF_HANDLERS_LIB}")
    endif()
    if(AF_RESPONSE_LIB)
        list(APPEND AF_COMMON_LIBS ${AF_RESPONSE_LIB})
        message(STATUS "Found af_response: ${AF_RESPONSE_LIB}")
    endif()
    if(AF_TRANSPORT_ADAPTERS_LIB)
        list(APPEND AF_COMMON_LIBS ${AF_TRANSPORT_ADAPTERS_LIB})
        message(STATUS "Found af_transport_adapters: ${AF_TRANSPORT_ADAPTERS_LIB}")
    endif()
    if(AF_HTTP_COMMUNICATION_LIB)
        list(APPEND AF_COMMON_LIBS ${AF_HTTP_COMMUNICATION_LIB})
        message(STATUS "Found af_http_communication: ${AF_HTTP_COMMUNICATION_LIB}")
    endif()
endif()

# Set PCF handler dependencies (shared between lib and standalone executable)
set(PCF_HANDLER_DEPS
    ${AF_COMMON_LIBS}
    oai::CONFIG
    oai::PCF
    oai::COMMON_MODEL
    oai::LOGGER
    oai::NAS
    oai::COMMON
    oai::UTILS
    gRPC::grpc++
    gRPC::grpc
    gRPC::grpc++_reflection
    gRPC::gpr
    protobuf::libprotobuf
    ${SPDLOG_TARGET}
    nlohmann_json::nlohmann_json
    yaml-cpp
    OpenSSL::Crypto
    OpenSSL::SSL
    nghttp2::nghttp2
    nghttp2::asio_http2
    Boost::system
    Boost::thread
    crypto
    # Abseil libraries required by gRPC
    ${ABSL_LINK_LIBRARIES}
)

# --- Configure the static library (for bundled build reuse) ---
target_include_directories(pcf_handler_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OAI_INCLUDE_DIRS}
        ${COMMON_INCLUDE_DIRS}
        /usr/local/include
        /usr/local/include/af
)

target_link_libraries(pcf_handler_lib
    PUBLIC
        ${PCF_HANDLER_DEPS}
)

target_link_options(pcf_handler_lib PUBLIC
    -Wl,--no-as-needed
    -L/usr/local/lib
)

# --- Configure the standalone executable ---
target_include_directories(pcf_server
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${OAI_INCLUDE_DIRS}
        ${COMMON_INCLUDE_DIRS}
        /usr/local/include
        /usr/local/include/af
)

target_link_libraries(pcf_server
    PUBLIC
        pcf_handler_lib
)

# Add linker options to ensure Abseil libraries are properly linked
target_link_options(pcf_server PRIVATE
    -Wl,--no-as-needed
    -L/usr/local/lib
)

# Install headers
install(DIRECTORY include/
    DESTINATION include/oai/af/southbound
    FILES_MATCHING PATTERN "*.h*"
)

install(TARGETS pcf_server
    RUNTIME DESTINATION bin
)

# Add tests if testing is enabled
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

message(STATUS "==========================================================")
message(STATUS "PCF Handler Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Using OAI libraries: ${OAI_FOUND}")
message(STATUS "  Using AF common: ${USE_INSTALLED_AF_COMMON}")
message(STATUS "==========================================================")
