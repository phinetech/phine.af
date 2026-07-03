# Root CMakeLists.txt for the 5G Application Function (AF) Microservice
#
# This is the main CMake configuration file that sets up the build for the entire
# AF microservice project. It includes common settings, compilation flags, and
# references to the individual component builds.
#
# Currently configured to build the common components only. As more components
# are implemented, they'll be added to this file.

cmake_minimum_required(VERSION 3.14)
project(phine.af VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable position independent code for all targets
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Export compile_commands.json for tooling (e.g. clang-tidy)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

# Find spdlog - prefer config mode to properly define all targets
if(NOT TARGET spdlog::spdlog)
    find_package(spdlog QUIET CONFIG)
    if(NOT TARGET spdlog::spdlog)
        # Fallback: manually create target from system installation
        find_path(SPDLOG_INCLUDE_DIR spdlog/spdlog.h)
        find_library(SPDLOG_LIBRARY NAMES spdlog)

        if(SPDLOG_INCLUDE_DIR AND SPDLOG_LIBRARY)
            add_library(spdlog::spdlog UNKNOWN IMPORTED)
            set_target_properties(spdlog::spdlog PROPERTIES
                IMPORTED_LOCATION "${SPDLOG_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${SPDLOG_INCLUDE_DIR}"
            )
            message(STATUS "Found spdlog (manual): ${SPDLOG_LIBRARY}")
        else()
            message(FATAL_ERROR "spdlog not found. Please install libspdlog-dev")
        endif()
    else()
        message(STATUS "Found spdlog via config file")
    endif()
else()
    message(STATUS "Using existing spdlog target")
endif()

# Components to build
add_subdirectory(common)

# Add northbound components (if directory exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/northbound)
    add_subdirectory(northbound)
endif()

# Add af core (if directory exists)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/af_core)
    add_subdirectory(af_core)
endif()

# Add southbound components (if directory exists)
option(ENABLE_PCF_HANDLER "Build the PCF southbound handler" ON)
option(ENABLE_NEF_HANDLER "Build the NEF southbound handler" OFF)
option(ENABLE_UDR_HANDLER "Build the UDR southbound handler" OFF)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/southbound)
    add_subdirectory(southbound)
endif()

# Testing configuration
option(BUILD_TESTS "Build test programs" OFF)
if(BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Bundled application configuration
# Build a single executable that bundles AF Core and southbound handlers.
# Northbound applications remain standalone services and connect to AF Core
# over the configured communication interface.
option(BUILD_BUNDLED "Build single bundled AF application" OFF)
if(BUILD_BUNDLED)
  message(STATUS "Building bundled AF application")

    add_executable(af
        src/bundled_af_runtime.cpp
        src/main.cpp
        src/southbound_handler_factory.cpp
    )

    if(ENABLE_PCF_HANDLER)
        target_compile_definitions(af PRIVATE AF_ENABLE_PCF_HANDLER=1)
    endif()

    if(ENABLE_NEF_HANDLER)
        target_compile_definitions(af PRIVATE AF_ENABLE_NEF_HANDLER=1)
    endif()

    if(ENABLE_UDR_HANDLER)
        target_compile_definitions(af PRIVATE AF_ENABLE_UDR_HANDLER=1)
    endif()

  target_include_directories(af
    PRIVATE
      ${CMAKE_SOURCE_DIR}
      ${CMAKE_SOURCE_DIR}/af_core/include
      ${CMAKE_SOURCE_DIR}/common
      ${CMAKE_SOURCE_DIR}/common/component/include
      ${CMAKE_SOURCE_DIR}/common/communication/include
      ${CMAKE_SOURCE_DIR}/common/models
      ${CMAKE_SOURCE_DIR}/common/models/include
      ${CMAKE_SOURCE_DIR}/common/config/include
      ${CMAKE_SOURCE_DIR}/common/di/include
      ${CMAKE_SOURCE_DIR}/common/cpp_utils/include
      ${CMAKE_SOURCE_DIR}/common/handlers/include
      ${CMAKE_SOURCE_DIR}/common/response/include
      ${CMAKE_SOURCE_DIR}/common/communication/adapters/include
  )

    if(ENABLE_PCF_HANDLER)
        target_include_directories(af
            PRIVATE
                ${CMAKE_SOURCE_DIR}/southbound/pcf_handler/include
        )
    endif()

    # Link bundled core + southbound libraries + common
  target_link_libraries(af
    PRIVATE
      af_core_lib
      af_common
  )

    # Note: pcf_handler_lib depends on OAI static libraries. We use whole-archive
    # to ensure all symbols from static libs are included in the final binary.
    if(ENABLE_PCF_HANDLER)
        target_link_libraries(af
            PRIVATE
                -Wl,--whole-archive
                pcf_handler_lib
                -Wl,--no-whole-archive
        )
    endif()

  # Ensure proper static library resolution (needed for transitive deps from static libs)
  target_link_options(af PRIVATE
    -Wl,--no-as-needed
    -L/usr/local/lib
  )

  install(TARGETS af
    RUNTIME DESTINATION bin
  )
endif()

# Print configuration summary
message(STATUS "Configuration Summary:")
message(STATUS "  Project version: ${PROJECT_VERSION}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build tests: ${BUILD_TESTS}")
message(STATUS "  PCF handler enabled: ${ENABLE_PCF_HANDLER}")
message(STATUS "  NEF handler enabled: ${ENABLE_NEF_HANDLER}")
message(STATUS "  UDR handler enabled: ${ENABLE_UDR_HANDLER}")