# CMakeLists.txt for Configuration Management
#
# This file configures the build for the configuration management component,
# which provides a way to load, access, and modify configuration parameters
# throughout the AF microservice application.

# Find required packages
find_package(yaml-cpp REQUIRED)

# Create library
add_library(af_config
    src/af_config.cpp
)

# Set include directories
target_include_directories(af_config
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(af_config
    PUBLIC
        yaml-cpp
)

# Set C++ standard
target_compile_features(af_config PUBLIC cxx_std_17)

# Install
install(TARGETS af_config
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/af/config
)