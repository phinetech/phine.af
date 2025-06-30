# CMakeLists.txt for PCF Handler module

cmake_minimum_required(VERSION 3.12)

# Set project name
project(pcf_handler VERSION 1.0.0 LANGUAGES CXX)

set(GIT_BRANCH        "UNKNOWN")
set(GIT_COMMIT_HASH   "UNKNOWN")
set(GIT_COMMIT_DATE   "UNKNOWN")
add_definitions("-DPACKAGE_VERSION=\"Branch: ${GIT_BRANCH} Abrev. Hash: ${GIT_COMMIT_HASH} Date: ${GIT_COMMIT_DATE}\"")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

# Common compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# TODO: Set compiler options using modern approach below
# target_compile_options(pcf_handler,$<$CONFIG:Debug:-g -O0>-Wall -Wextra)

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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

# Add CMake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/cmake)

# Include custom CMake modules
include(Protobuf)

# Find dependencies
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(NGHTTP2 REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)
find_package(OpenSSL REQUIRED) # for TLS support

# Define source files
set(SOURCES
    ${CONFIG_SRC_FILES}
    src/pcf_handler.cpp
    src/pcc_rule_manager.cpp
    src/pcf_client_wrapper.cpp
)

# Include directories
include_directories(
    include
    ${CMAKE_SOURCE_DIR}/common/cpp_utils
    ${CMAKE_SOURCE_DIR}/common/component/include
    ${CMAKE_SOURCE_DIR}/common/communication/include
    ${CMAKE_SOURCE_DIR}/common/models
    ${CMAKE_SOURCE_DIR}/common/di/include
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

# Components to build
add_subdirectory(common)
ADD_SUBDIRECTORY(${COMMON_DIR} ${CMAKE_CURRENT_BINARY_DIR}/oai-common)
ADD_SUBDIRECTORY(${LOGGER_DIR} ${CMAKE_CURRENT_BINARY_DIR}/oai-logger)
ADD_SUBDIRECTORY(${UTILS_DIR} ${CMAKE_CURRENT_BINARY_DIR}/oai-utils)
ADD_SUBDIRECTORY(${NAS_DIR} ${CMAKE_CURRENT_BINARY_DIR}/oai-nas)

# TODO: ensure logger librabry is built for the PCF Handler

# Create shared library
add_library(pcf_handler STATIC ${SOURCES})

add_library(CONFIG STATIC ${CONFIG_SRC_FILES})
add_library(PCF STATIC ${USED_PCF_MODEL_SRC_FILES})
add_library(COMMON_MODEL STATIC ${USED_COMMON_MODEL_SRC_FILES} ${CONFIG_SRC_FILES})
# target_include_directories(COMMON_MODEL PUBLIC ${COMMON_MODEL_DIR})


# # Link dependencies
target_link_libraries(pcf_handler
    PUBLIC
        CONFIG
        PCF
        COMMON_MODEL
        LOGGER
        NAS
        fmt
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        # TODO: use namespaced targets for these
        yaml-cpp
        nghttp2
        Boost::system
        Boost::thread
        protobuf::libprotobuf
        gRPC::grpc++
        af_common_component
        af_communication
        # ssl
        # crypto
)

# Create executable
add_executable(pcf_server src/main.cpp)

# Link library to executable
target_link_libraries(pcf_server
    PUBLIC
        pcf_handler
        ${NGHTTP2_LIBRARIES}
        Boost::system
        Boost::thread
        nghttp2_asio
        ssl
        crypto
        -Wl,-Bdynamic # TODO remove this
)

# Install targets
install(TARGETS pcf_handler
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY include/
    DESTINATION include/oai/af/southbound
    FILES_MATCHING PATTERN "*.h*"
)

install(TARGETS pcf_server
    RUNTIME DESTINATION bin
)

install(FILES ${HEADERS}
    DESTINATION include/pcf_handler
)

# Add tests if testing is enabled
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()