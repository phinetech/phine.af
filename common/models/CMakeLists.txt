# CMakeLists.txt for Common Models
#
# This file configures the build for common data models used throughout
# the AF microservice application, such as the Message model.

# Create library for common models
add_library(af_models INTERFACE)

# Set include directories
target_include_directories(af_models INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/af/models
    FILES_MATCHING PATTERN "*.h"
)