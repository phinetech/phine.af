# Root CMakeLists.txt for the 5G Application Function (AF) Microservice
#
# This is the main CMake configuration file that sets up the build for the entire
# AF microservice project. It includes common settings, compilation flags, and
# references to the individual component builds.
#
# Currently configured to build the common components only. As more components
# are implemented, they'll be added to this file.

cmake_minimum_required(VERSION 3.14)
project(oai-cn5g-af VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add CMake modules path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/cmake)

# Include custom CMake modules
include(Protobuf)

# Build type
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build" FORCE)
endif()
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Common compilation flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Output directories
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Dependencies
find_package(Threads REQUIRED)
find_package(gRPC REQUIRED)

# Optional: Find and use pre-installed protobuf or build from source
find_package(Protobuf)
if(NOT Protobuf_FOUND)
  message(STATUS "Protobuf not found. Will use third-party/protobuf if available.")
endif()

# Components to build
add_subdirectory(common)

# Add northbound components
add_subdirectory(northbound)

# Add af core
add_subdirectory(af_core)

# Add southbound components
add_subdirectory(southbound)

# Testing configuration
option(BUILD_TESTS "Build test programs" OFF)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Print configuration summary
message(STATUS "Configuration Summary:")
message(STATUS "  Project version: ${PROJECT_VERSION}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build tests: ${BUILD_TESTS}")