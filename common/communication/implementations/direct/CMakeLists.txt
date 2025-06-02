# CMakeLists.txt for Direct Communication Implementation
#
# This file configures the build for the direct function call implementation
# of the communication service. This implementation is used for in-process
# communication between AF components.

# Add position-independent code flag
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Create library
add_library(af_direct_communication
    src/direct_communication_service.cpp
)

# Set include directories
target_include_directories(af_direct_communication
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../    # Add common directory for proper include resolution
)

# Link dependencies
target_link_libraries(af_direct_communication
    PUBLIC
        af_communication_interface
    PRIVATE
        af_cpp_utils
)

# Install
install(TARGETS af_direct_communication
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/af/communication/implementations/direct
)