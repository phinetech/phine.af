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
find_package(nlohmann_json REQUIRED)
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

# Find spdlog - use same mode as OAI libraries (header-only by default)
find_package(spdlog REQUIRED)

# Prefer header-only mode to match OAI library build
if(TARGET spdlog::spdlog_header_only)
    set(SPDLOG_TARGET spdlog::spdlog_header_only)
    message(STATUS "Using spdlog header-only mode")
elseif(TARGET spdlog::spdlog)
    set(SPDLOG_TARGET spdlog::spdlog)
    message(STATUS "Using spdlog compiled library mode")
else()
    message(FATAL_ERROR "spdlog target not found")
endif()

find_package(yaml-cpp REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(NGHTTP2 REQUIRED)
find_package(NGHTTP2_ASIO REQUIRED)

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
add_library(pcf_handler STATIC ${SOURCES})

# Set include directories based on whether we're using installed library or not
if(USE_INSTALLED_AF_COMMON)
    target_include_directories(pcf_handler
        PUBLIC
            include
            ${AF_COMMON_INCLUDE_DIR}
            ${OAI_INCLUDE_DIRS}
    )
else()
    target_include_directories(pcf_handler
        PUBLIC
            include
            ${OAI_INCLUDE_DIRS}
    )
endif()

# Link libraries
if(USE_INSTALLED_AF_COMMON)
    target_link_libraries(pcf_handler
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
    )
else()
    target_link_libraries(pcf_handler
        PUBLIC
            af_common
            af_common_component
            af_communication
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
    )
endif()

# Create executable
add_executable(pcf_server src/main.cpp)

# Add include directories for the executable
target_include_directories(pcf_server
    PRIVATE
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

# Link library to executable  
target_link_libraries(pcf_server
    PUBLIC
        pcf_handler
        gRPC::grpc
        gRPC::grpc++
        ${NGHTTP2_LIBRARIES}
        Boost::system
        Boost::thread
        nghttp2_asio
        ssl
        crypto
        z
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
