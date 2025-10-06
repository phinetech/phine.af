# CMakeLists.txt for AF Core module

cmake_minimum_required(VERSION 3.10)

# Set project name and version
project(af_core VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
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

# Find dependencies
find_package(spdlog REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(yaml-cpp REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Define source files
set(SOURCES
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

# Include directories
include_directories(
    include
    ${CMAKE_SOURCE_DIR}/common/cpp_utils
    ${CMAKE_SOURCE_DIR}/common/component/include
    ${CMAKE_SOURCE_DIR}/common/communication/include
    ${CMAKE_SOURCE_DIR}/common/models
    ${CMAKE_SOURCE_DIR}/common/di/include
)

# Components to build
add_subdirectory(common)

# Create shared library
add_library(af_core_lib SHARED ${SOURCES})

# Link dependencies
target_link_libraries(af_core_lib
    PUBLIC
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        yaml-cpp
        Boost::system
        Boost::thread
        protobuf::libprotobuf
        gRPC::grpc++
        af_common_component
        af_communication
)

# Create executable
add_executable(af_core src/main.cpp)

# Link library to executable
target_link_libraries(af_core
    PRIVATE
        af_core_lib
)

# Install targets
install(TARGETS af_core_lib
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS af_core
    RUNTIME DESTINATION bin
)

install(FILES ${HEADERS}
    DESTINATION include/af_core
)

# Add tests if testing is enabled
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()