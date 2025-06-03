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

# Components to build
add_subdirectory(common)

# Create library
add_library(api_component SHARED
    src/api_adapter.cpp
    src/api_handlers.cpp
)

# Link dependencies
target_link_libraries(api_component
    PUBLIC
        ${NGHTTP2_INCLUDE_DIRS}
        Boost::system
        Boost::thread
        nghttp2_asio
        OpenSSL::SSL
        OpenSSL::Crypto
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        yaml-cpp
        # crypto  # Add OpenSSL crypto library
        # ssl  
        af_common_component
        af_communication
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
target_link_libraries(api_server 
    PRIVATE
        ${NGHTTP2_LIBRARIES}
        Boost::system
        Boost::thread
        api_component
        nghttp2_asio
        OpenSSL::SSL  # Explicitly link OpenSSL SSL library
        OpenSSL::Crypto  # Explicitly link OpenSSL Crypto library
)

# Installation for the executable
install(TARGETS api_server
    RUNTIME DESTINATION bin
)