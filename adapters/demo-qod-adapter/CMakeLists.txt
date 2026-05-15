# CMakeLists.txt for Demo QoD Adapter
#
# Builds a standalone C++ adapter that communicates with af_core via gRPC.
# Dependencies: gRPC, Protobuf, nlohmann_json, spdlog, yaml-cpp
#
# The adapter generates its own protobuf stubs from common/protos/message.proto
# and does NOT link against af_common.

cmake_minimum_required(VERSION 3.14)
project(demo_qod_adapter VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# ─── Find dependencies ───────────────────────────────────────────────────────

find_package(Threads REQUIRED)

# Protobuf
find_package(Protobuf CONFIG QUIET)
if(NOT Protobuf_FOUND)
    find_package(Protobuf REQUIRED)
endif()

# Create alias if needed
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

# gRPC
find_package(gRPC CONFIG QUIET)
if(NOT gRPC_FOUND)
    # Fallback: find gRPC manually
    find_library(GRPC_LIBRARY NAMES grpc++)
    find_path(GRPC_INCLUDE_DIR grpcpp/grpcpp.h)
    if(GRPC_LIBRARY AND GRPC_INCLUDE_DIR)
        add_library(gRPC::grpc++ UNKNOWN IMPORTED)
        set_target_properties(gRPC::grpc++ PROPERTIES
            IMPORTED_LOCATION "${GRPC_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${GRPC_INCLUDE_DIR}"
        )
        set(gRPC_FOUND TRUE)
    else()
        message(FATAL_ERROR "gRPC not found")
    endif()
endif()

# gRPC reflection (optional, not required for client)
if(NOT TARGET gRPC::grpc++_reflection)
    find_library(GRPC_REFLECTION_LIBRARY NAMES grpc++_reflection)
    if(GRPC_REFLECTION_LIBRARY)
        add_library(gRPC::grpc++_reflection UNKNOWN IMPORTED)
        set_target_properties(gRPC::grpc++_reflection PROPERTIES
            IMPORTED_LOCATION "${GRPC_REFLECTION_LIBRARY}"
        )
    endif()
endif()

# nlohmann_json (header-only)
if(NOT TARGET nlohmann_json::nlohmann_json)
    find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
    if(NLOHMANN_JSON_INCLUDE_DIR)
        add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
        set_target_properties(nlohmann_json::nlohmann_json PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_INCLUDE_DIR}"
        )
    else()
        message(FATAL_ERROR "nlohmann_json not found. Install nlohmann-json3-dev")
    endif()
endif()

# spdlog + fmt — the installed spdlog lacks bundled fmt headers,
# so we define SPDLOG_FMT_EXTERNAL and link against the system fmt library.
if(NOT TARGET fmt::fmt)
    find_path(FMT_INCLUDE_DIR fmt/core.h)
    find_library(FMT_LIBRARY NAMES fmt)
    if(FMT_INCLUDE_DIR AND FMT_LIBRARY)
        add_library(fmt::fmt UNKNOWN IMPORTED)
        set_target_properties(fmt::fmt PROPERTIES
            IMPORTED_LOCATION "${FMT_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${FMT_INCLUDE_DIR}"
        )
        message(STATUS "Found fmt: ${FMT_LIBRARY}")
    else()
        message(FATAL_ERROR "fmt not found. Install libfmt-dev")
    endif()
endif()

if(NOT TARGET spdlog::spdlog)
    find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h)
    find_library(SPDLOG_LIBRARY NAMES spdlog)
    if(SPDLOG_INCLUDE_DIR AND SPDLOG_LIBRARY)
        add_library(spdlog::spdlog UNKNOWN IMPORTED)
        set_target_properties(spdlog::spdlog PROPERTIES
            IMPORTED_LOCATION "${SPDLOG_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SPDLOG_INCLUDE_DIR}"
            INTERFACE_COMPILE_DEFINITIONS "SPDLOG_FMT_EXTERNAL"
            INTERFACE_LINK_LIBRARIES "fmt::fmt"
        )
        message(STATUS "Found spdlog: ${SPDLOG_LIBRARY}")
        message(STATUS "spdlog include: ${SPDLOG_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "spdlog not found. Install libspdlog-dev")
    endif()
endif()

# yaml-cpp
if(NOT TARGET yaml-cpp)
    find_path(YAML_CPP_INCLUDE_DIR yaml-cpp/yaml.h)
    find_library(YAML_CPP_LIBRARY NAMES yaml-cpp)
    if(YAML_CPP_INCLUDE_DIR AND YAML_CPP_LIBRARY)
        add_library(yaml-cpp UNKNOWN IMPORTED)
        set_target_properties(yaml-cpp PROPERTIES
            IMPORTED_LOCATION "${YAML_CPP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${YAML_CPP_INCLUDE_DIR}"
        )
    else()
        message(FATAL_ERROR "yaml-cpp not found. Install libyaml-cpp-dev")
    endif()
endif()

# ─── Generate protobuf/gRPC stubs from message.proto ─────────────────────────

set(PROTO_GEN_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

set(PROTO_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../../common/protos/message.proto)
get_filename_component(PROTO_FILE ${PROTO_FILE} ABSOLUTE)
get_filename_component(PROTO_PATH ${PROTO_FILE} DIRECTORY)

# Find protoc
if(TARGET protobuf::protoc)
    get_target_property(PROTOC_BIN protobuf::protoc IMPORTED_LOCATION)
    if(NOT PROTOC_BIN)
        get_target_property(PROTOC_BIN protobuf::protoc LOCATION)
    endif()
endif()
if(NOT PROTOC_BIN)
    set(PROTOC_BIN ${Protobuf_PROTOC_EXECUTABLE})
endif()
if(NOT PROTOC_BIN)
    find_program(PROTOC_BIN protoc)
endif()

# Find grpc_cpp_plugin
find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin
    PATHS /usr/local/bin /usr/bin
    NO_DEFAULT_PATH)
if(NOT GRPC_CPP_PLUGIN)
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
endif()

message(STATUS "protoc: ${PROTOC_BIN}")
message(STATUS "grpc_cpp_plugin: ${GRPC_CPP_PLUGIN}")

set(PROTO_SRCS ${PROTO_GEN_DIR}/message.pb.cc)
set(PROTO_HDRS ${PROTO_GEN_DIR}/message.pb.h)
set(GRPC_SRCS ${PROTO_GEN_DIR}/message.grpc.pb.cc)
set(GRPC_HDRS ${PROTO_GEN_DIR}/message.grpc.pb.h)

add_custom_command(
    OUTPUT ${PROTO_SRCS} ${PROTO_HDRS} ${GRPC_SRCS} ${GRPC_HDRS}
    COMMAND ${PROTOC_BIN}
    ARGS --cpp_out=${PROTO_GEN_DIR}
         --grpc_out=${PROTO_GEN_DIR}
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         -I${PROTO_PATH}
         ${PROTO_FILE}
    DEPENDS ${PROTO_FILE}
    COMMENT "Generating gRPC/Protobuf stubs from message.proto"
    VERBATIM
)

add_library(adapter_protos STATIC
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    ${GRPC_SRCS}
    ${GRPC_HDRS}
)

target_include_directories(adapter_protos
    PUBLIC
        ${PROTO_GEN_DIR}
)

target_link_libraries(adapter_protos
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

set_target_properties(adapter_protos PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
)

# ─── Adapter library (shared between main and tests) ─────────────────────────

add_library(adapter_lib STATIC
    src/qod_client.cpp
    src/session_manager.cpp
)

target_include_directories(adapter_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(adapter_lib
    PUBLIC
        adapter_protos
        gRPC::grpc++
        protobuf::libprotobuf
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        yaml-cpp
        Threads::Threads
)

set_target_properties(adapter_lib PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_STANDARD 17
)

# ─── Main executable ─────────────────────────────────────────────────────────

add_executable(demo_qod_adapter src/main.cpp)

target_link_libraries(demo_qod_adapter
    PRIVATE
        adapter_lib
)

# ─── Tests (optional) ────────────────────────────────────────────────────────

option(BUILD_ADAPTER_TESTS "Build adapter unit tests" OFF)

if(BUILD_ADAPTER_TESTS)
    enable_testing()

    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(adapter_tests
        tests/test_qod_client.cpp
        tests/test_session_manager.cpp
    )

    target_link_libraries(adapter_tests
        PRIVATE
            adapter_lib
            GTest::gtest_main
            GTest::gmock
    )

    include(GoogleTest)
    gtest_discover_tests(adapter_tests)
endif()

# ─── Summary ─────────────────────────────────────────────────────────────────

message(STATUS "")
message(STATUS "=== Demo QoD Adapter Configuration ===")
message(STATUS "  Version:    ${PROJECT_VERSION}")
message(STATUS "  Proto file: ${PROTO_FILE}")
message(STATUS "  Gen dir:    ${PROTO_GEN_DIR}")
message(STATUS "  Tests:      ${BUILD_ADAPTER_TESTS}")
message(STATUS "")
