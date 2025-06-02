# CMakeLists.txt for the Dependency Injection Framework
#
# This file configures the build for the AF's dependency injection framework.
# The framework provides service location, registration, and lifetime management
# capabilities that are essential for the AF's modular architecture.
#
# The library includes:
# - ServiceProvider interface for resolving dependencies
# - ServiceFactory interface and implementations for creating services
# - ServiceContainer for registering and managing services
#
# This library is used by all AF components to manage their dependencies.

cmake_minimum_required(VERSION 3.10)

# Create library for DI framework
add_library(af_di
    src/service_container.cpp
    src/service_provider.cpp
)

# Set include directories
target_include_directories(af_di
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Set C++ standard
target_compile_features(af_di PUBLIC cxx_std_17)

# Install
install(TARGETS af_di
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/af/di
)