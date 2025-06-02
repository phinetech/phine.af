# CMakeLists.txt for the base component module
cmake_minimum_required(VERSION 3.10)
project(af_common_component VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files
set(SOURCES
    src/af_component.cpp
)

# Include directories
include_directories(
    include
)

# Find dependencies
find_package(spdlog REQUIRED)

# Create library
add_library(af_common_component SHARED ${SOURCES})

# Set library properties
set_target_properties(af_common_component PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# Link dependencies
target_link_libraries(af_common_component
    PUBLIC
        spdlog::spdlog
)

# Specify include directories for consumers of this library
target_include_directories(af_common_component
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Install targets
install(TARGETS af_common_component
    EXPORT af_common_component-targets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(DIRECTORY include/
    DESTINATION include/oai/af/common/component
    FILES_MATCHING PATTERN "*.h*"
)

# Install export targets
install(EXPORT af_common_component-targets
    FILE af_common_component-targets.cmake
    NAMESPACE af::
    DESTINATION lib/cmake/af_common_component
)