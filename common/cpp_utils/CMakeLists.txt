# CMakeLists.txt for C++ Utilities
#
# This file configures the build for common C++ utilities used throughout
# the AF microservice application, including error handling and utility classes.

# Create library for C++ utilities
add_library(af_cpp_utils INTERFACE)

# Set include directories
target_include_directories(af_cpp_utils INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Install headers
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include/af/cpp_utils
    FILES_MATCHING PATTERN "*.h"
)