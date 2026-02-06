# CMakeLists.txt for api_component
cmake_minimum_required(VERSION 3.10)
project(api_component VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Common compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Add CMake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/cmake)

# Include custom CMake modules
include(Protobuf)

# Source files
set(SOURCES
    src/api_adapter.cpp
    src/api_handlers.cpp
    src/main.cpp
)

# Include directories
include_directories(
    include
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/common/cpp_utils
    ${CMAKE_SOURCE_DIR}/common/component/include
    ${CMAKE_SOURCE_DIR}/common/communication/include
    ${CMAKE_SOURCE_DIR}/common/models
    ${CMAKE_SOURCE_DIR}/common/di/include
)

# Find dependencies
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(OpenSSL REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(NGHTTP2 REQUIRED)
# Find nghttp2_asio
# find_package(nghttp2_asio REQUIRED)
# Find gRPC
find_package(gRPC REQUIRED)
# FInd protobuf
find_package(Protobuf REQUIRED)


# Include the nghttp2_asio headers
# target_include_directories(api_component PRIVATE ${NGHTTP2_INCLUDE_DIRS})
# Print nghttp2 include directories
message(STATUS "NGHTTP2 include directories: ${NGHTTP2_INCLUDE_DIRS}")
message(STATUS "gRPC include directories: ${gRPC_INCLUDE_DIRS}")

# When building standalone (Docker), find installed AF common libraries
# Check if we're in Docker or standalone build (common libraries already installed)
if(NOT TARGET af_common_component)
    message(STATUS "af_common_component target not found, searching for installed libraries")

    # Find installed headers - when common is installed, headers go to /usr/local/include
    find_path(AF_COMMON_INCLUDE_DIR af_component.h
        PATHS
            /usr/local/include
            /usr/local/include/oai/af/common
            /app/common/component/include
            ${CMAKE_SOURCE_DIR}/common/component/include
    )

    find_path(AF_COMMUNICATION_INCLUDE_DIR communication_interface.h
        PATHS
            /usr/local/include
            /usr/local/include/oai/af/common
            /app/common/communication/include
            ${CMAKE_SOURCE_DIR}/common/communication/include
    )

    find_path(AF_CPP_UTILS_DIR cpp_string.hpp
        PATHS
            /usr/local/include
            /usr/local/include/oai/af/common
            /app/common/cpp_utils
            ${CMAKE_SOURCE_DIR}/common/cpp_utils
    )

    find_path(AF_MODELS_DIR message.h
        PATHS
            /usr/local/include
            /usr/local/include/oai/af/common
            /app/common/models
            ${CMAKE_SOURCE_DIR}/common/models
    )

    find_path(AF_DI_INCLUDE_DIR di_container.h
        PATHS
            /usr/local/include
            /usr/local/include/oai/af/common
            /app/common/di/include
            ${CMAKE_SOURCE_DIR}/common/di/include
    )

    # Find installed libraries
    find_library(AF_COMMON_COMPONENT_LIB
        NAMES af_common_component
        PATHS /usr/local/lib /app/build-output/lib
    )

    find_library(AF_COMMUNICATION_FACTORY_LIB
        NAMES af_communication_factory
        PATHS /usr/local/lib /app/build-output/lib
    )

    find_library(AF_DIRECT_COMMUNICATION_LIB
        NAMES af_direct_communication
        PATHS /usr/local/lib /app/build-output/lib
    )

    find_library(AF_GRPC_COMMUNICATION_LIB
        NAMES af_grpc_communication
        PATHS /usr/local/lib /app/build-output/lib
    )

    find_library(AF_PROTOS_LIB
        NAMES af_protos
        PATHS /usr/local/lib /app/build-output/lib
    )

    if(AF_COMMON_INCLUDE_DIR AND AF_COMMUNICATION_INCLUDE_DIR)
        message(STATUS "Found AF common headers: ${AF_COMMON_INCLUDE_DIR}")
        message(STATUS "Found AF communication headers: ${AF_COMMUNICATION_INCLUDE_DIR}")

        # Add include directories for installed headers
        # Only add directories that were found
        set(AF_INCLUDE_DIRS ${AF_COMMON_INCLUDE_DIR} ${AF_COMMUNICATION_INCLUDE_DIR})

        if(AF_CPP_UTILS_DIR)
            list(APPEND AF_INCLUDE_DIRS ${AF_CPP_UTILS_DIR})
        endif()
        if(AF_MODELS_DIR)
            list(APPEND AF_INCLUDE_DIRS ${AF_MODELS_DIR})
        endif()
        if(AF_DI_INCLUDE_DIR)
            list(APPEND AF_INCLUDE_DIRS ${AF_DI_INCLUDE_DIR})
        endif()

        include_directories(${AF_INCLUDE_DIRS})

        # Create imported targets
        if(AF_COMMON_COMPONENT_LIB)
            add_library(af_common_component SHARED IMPORTED)
            set_target_properties(af_common_component PROPERTIES
                IMPORTED_LOCATION ${AF_COMMON_COMPONENT_LIB}
            )
            message(STATUS "Found af_common_component: ${AF_COMMON_COMPONENT_LIB}")
        endif()

        if(AF_COMMUNICATION_FACTORY_LIB)
            add_library(af_communication_factory SHARED IMPORTED)
            set_target_properties(af_communication_factory PROPERTIES
                IMPORTED_LOCATION ${AF_COMMUNICATION_FACTORY_LIB}
            )
            message(STATUS "Found af_communication_factory: ${AF_COMMUNICATION_FACTORY_LIB}")
        endif()

        if(AF_DIRECT_COMMUNICATION_LIB)
            add_library(af_direct_communication SHARED IMPORTED)
            set_target_properties(af_direct_communication PROPERTIES
                IMPORTED_LOCATION ${AF_DIRECT_COMMUNICATION_LIB}
            )
            message(STATUS "Found af_direct_communication: ${AF_DIRECT_COMMUNICATION_LIB}")
        endif()

        if(AF_GRPC_COMMUNICATION_LIB)
            add_library(af_grpc_communication STATIC IMPORTED)
            set_target_properties(af_grpc_communication PROPERTIES
                IMPORTED_LOCATION ${AF_GRPC_COMMUNICATION_LIB}
            )
            message(STATUS "Found af_grpc_communication: ${AF_GRPC_COMMUNICATION_LIB}")
        endif()

        if(AF_PROTOS_LIB)
            add_library(af_protos SHARED IMPORTED)
            set_target_properties(af_protos PROPERTIES
                IMPORTED_LOCATION ${AF_PROTOS_LIB}
            )
            message(STATUS "Found af_protos: ${AF_PROTOS_LIB}")
        endif()
    else()
        message(FATAL_ERROR "AF common libraries not found. Please build common first.")
    endif()
endif()

# Create library
add_library(api_component SHARED
    src/api_adapter.cpp
    src/api_handlers.cpp
)

# Add nghttp2 include directories
target_include_directories(api_component PUBLIC ${NGHTTP2_INCLUDE_DIRS})

# Link dependencies
target_link_libraries(api_component
    PUBLIC
        Boost::system
        Boost::thread
        nghttp2_asio
        OpenSSL::SSL
        OpenSSL::Crypto
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        yaml-cpp
        af_common_component
        af_communication_factory
)

# Install targets
install(TARGETS api_component
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY include/
    DESTINATION include/oai/af/northbound
    FILES_MATCHING PATTERN "*.h*"
)

# Main executable for standalone operation
add_executable(api_server src/main.cpp)

# For static library af_grpc_communication, we need to use whole-archive to include all symbols
if(AF_GRPC_COMMUNICATION_LIB)
    target_link_libraries(api_server
        PRIVATE
            api_component
            af_communication_factory
            -Wl,--whole-archive
            ${AF_GRPC_COMMUNICATION_LIB}
            -Wl,--no-whole-archive
            af_protos
            af_direct_communication
            gRPC::grpc++
            gRPC::grpc++_reflection
            gRPC::grpc
            gRPC::gpr
            protobuf::libprotobuf
            ${NGHTTP2_LIBRARIES}
            Boost::system
            Boost::thread
            nghttp2_asio
            OpenSSL::SSL
            OpenSSL::Crypto
    )
else()
    target_link_libraries(api_server
        PRIVATE
            api_component
            af_communication_factory
            af_grpc_communication
            af_protos
            af_direct_communication
            gRPC::grpc++
            gRPC::grpc++_reflection
            gRPC::grpc
            gRPC::gpr
            protobuf::libprotobuf
            ${NGHTTP2_LIBRARIES}
            Boost::system
            Boost::thread
            nghttp2_asio
            OpenSSL::SSL
            OpenSSL::Crypto
    )
endif()

# Installation for the executable
install(TARGETS api_server
    RUNTIME DESTINATION bin
)