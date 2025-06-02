# CMakeLists.txt for PCF Handler module

cmake_minimum_required(VERSION 3.12)

# Set project name
project(pcf_handler VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compile options
add_compile_options(-Wall -Wextra -Wpedantic)

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
    src/pcf_handler.cpp
    src/pcc_rule_manager.cpp
    src/pcf_client_wrapper.cpp
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

# Create shared library
add_library(pcf_handler_lib SHARED ${SOURCES})

# Link dependencies
target_link_libraries(pcf_handler_lib
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        yaml-cpp
        nghttp2
        Boost::system
        Boost::thread
        protobuf::libprotobuf
        gRPC::grpc++
        af_common_component
        af_communication
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Create executable
add_executable(pcf_handler src/main.cpp)

# Link library to executable
target_link_libraries(pcf_handler
    PRIVATE
        pcf_handler_lib
)

# Install targets
install(TARGETS pcf_handler_lib
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS pcf_handler
    RUNTIME DESTINATION bin
)

install(FILES ${HEADERS}
    DESTINATION include/pcf_handler
)

# Add tests if testing is enabled
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()