# CMakeLists.txt for PCF Handler module

cmake_minimum_required(VERSION 3.14)
project(pcf_handler VERSION 1.0.0 LANGUAGES CXX)

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
find_package(nlohmann_json REQUIRED)
find_package(OpenSSL REQUIRED)

# Set OPENSSL_ROOT_DIR for nghttp2 if needed
if(DEFINED OpenSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR ${OpenSSL_ROOT_DIR} CACHE PATH "Path to OpenSSL root directory" FORCE)
endif()

find_package(spdlog REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(NGHTTP2 REQUIRED)
# TODO: enable once FindNGHTTP2_ASIO.cmake is finalised
# find_package(NGHTTP2_ASIO REQUIRED)

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

# Set path variables used in compilation
set (SRC_TOP_DIR   "${CMAKE_CURRENT_SOURCE_DIR}")
set (MOUNTED_COMMON      oai-cn5g-common-src)
set (LOGGER_DIR     ${SRC_TOP_DIR}/${MOUNTED_COMMON}/logger)
set (CONFIG_DIR     ${SRC_TOP_DIR}/${MOUNTED_COMMON}/config)
set (COMMON_DIR     ${SRC_TOP_DIR}/${MOUNTED_COMMON}/common)
set (UTILS_DIR      ${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils)
set (3GPP_DIR      ${SRC_TOP_DIR}/${MOUNTED_COMMON}/3gpp)
set (NAS_DIR        ${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas)
set (PCF_MODEL_DIR  ${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/pcf)

# Define source files
set(SOURCES
    ${CONFIG_SRC_FILES}
    src/helpers.cpp
    src/qod_pcf_handler.cpp
    src/request_router.cpp
    src/pcf_handler.cpp
    src/pcc_rule_manager.cpp
    src/pcf_client_wrapper.cpp
)

include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/3gpp)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/config)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/logger)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/nas)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/pcf)
include_directories(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/common_model)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/3gpp/3gpp.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/http/http.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/logger/logger.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/utils/utils.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/config/config.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/common_model/common_model.cmake)
include(${SRC_TOP_DIR}/${MOUNTED_COMMON}/model/pcf/pcf_model.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/build/cmake/used_common_files.cmake)

# Create test executable
add_executable(pcf_handler ${SOURCES})

# Set include directories based on whether we're using installed library or not
if(USE_INSTALLED_AF_COMMON)
    target_include_directories(pcf_handler
        PRIVATE
            include
            ${AF_COMMON_INCLUDE_DIR}
    )
else()
    target_include_directories(pcf_handler
        PRIVATE
            include
    )
endif()

# Link libraries
if(USE_INSTALLED_AF_COMMON)
    target_link_libraries(pcf_handler
        PRIVATE
            ${AF_COMMON_LIB}
            ${AF_PROTOS_LIB}
            gRPC::grpc++
            gRPC::grpc++_reflection
            protobuf::libprotobuf
            nlohmann_json::nlohmann_json
            OpenSSL::SSL
            OpenSSL::Crypto
            nghttp2::nghttp2
            nghttp2_asio::nghttp2_asio
            Boost::system
            Boost::thread
    )
else()
    target_link_libraries(pcf_handler
        PRIVATE
            af_common
            gRPC::grpc++
            gRPC::grpc++_reflection
            protobuf::libprotobuf
            nlohmann_json::nlohmann_json
            OpenSSL::SSL
            OpenSSL::Crypto
            nghttp2::nghttp2
            nghttp2_asio::nghttp2_asio
            Boost::system
            Boost::thread
    )
endif()