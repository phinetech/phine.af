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

# Add custom cmake module path
# In Docker builds, common is copied to the same directory level
# Otherwise, it's at ../common (one level up in the southbound directory)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/common/cmake")
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/common/cmake")
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

# Try to find the installed af_common library
find_library(AF_COMMON_LIB NAMES af_common libaf_common)
find_library(AF_PROTOS_LIB NAMES af_protos libaf_protos)
find_library(AF_COMMUNICATION_LIB NAMES af_communication libaf_communication)
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

# Define source files (only pcf_handler specific sources)
set(SOURCES
    src/main.cpp
    src/helpers.cpp
    src/qod_pcf_handler.cpp
    src/request_router.cpp
    src/pcf_handler.cpp
    src/pcc_rule_manager.cpp
    src/pcf_client_wrapper.cpp
)

# Create shared library
# add_library(pcf_handler STATIC ${SOURCES})
# Create test executable
add_executable(pcf_server ${SOURCES})

# Set include directories based on whether we're using installed library or not
if(USE_INSTALLED_AF_COMMON)
    target_include_directories(pcf_server
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/common
            ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp_utils
            ${CMAKE_CURRENT_SOURCE_DIR}/common/models
            ${CMAKE_CURRENT_SOURCE_DIR}/common/communication/include
            ${CMAKE_CURRENT_SOURCE_DIR}/common/component/include
            ${CMAKE_CURRENT_SOURCE_DIR}/common/di/include
            ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp_utils/include
            ${OAI_INCLUDE_DIRS}
    )
else()
    target_include_directories(pcf_server
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/common
            ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp_utils
            ${CMAKE_CURRENT_SOURCE_DIR}/common/models
            ${CMAKE_CURRENT_SOURCE_DIR}/common/communication/include
            ${CMAKE_CURRENT_SOURCE_DIR}/common/component/include
            ${CMAKE_CURRENT_SOURCE_DIR}/common/di/include
            ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp_utils/include
            ${OAI_INCLUDE_DIRS}
    )
endif()

# Link libraries
if(USE_INSTALLED_AF_COMMON)
    target_link_libraries(pcf_server
        PUBLIC
            ${AF_COMMON_LIB}
            ${AF_PROTOS_LIB}
            ${AF_COMMUNICATION_LIB}
            oai::CONFIG
            oai::PCF
            oai::COMMON_MODEL
            oai::LOGGER
            oai::NAS
            oai::COMMON
            oai::UTILS
            gRPC::grpc++
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
else()
    target_link_libraries(pcf_server
        PUBLIC
            af_common
            af_common_component
            af_communication
            af_protos
            oai::CONFIG
            oai::PCF
            oai::COMMON_MODEL
            oai::LOGGER
            oai::NAS
            oai::COMMON
            oai::UTILS
            gRPC::grpc++
            gRPC::grpc++_reflection
            gRPC::gpr
            protobuf::libprotobuf
            ${SPDLOG_TARGET}
            fmt
            nlohmann_json::nlohmann_json
            yaml-cpp
            OpenSSL::Crypto
            OpenSSL::SSL
            nghttp2::nghttp2
            nghttp2::asio_http2
            Boost::system
            Boost::thread
            crypto
            # Abseil libraries required by gRPC - must come after static libs
            ${ABSL_LINK_LIBRARIES}
    )
endif()

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
