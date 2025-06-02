# CMakeLists.txt for Communication Abstraction Layer
#
# This file configures the build for the communication abstraction layer
# which provides a unified interface for different communication mechanisms
# between AF components.

# Add position-independent code flag
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Create interface library for communication interface
add_library(af_communication_interface INTERFACE)
target_include_directories(af_communication_interface INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/..    # Add parent directory (common) for proper include resolution
)
target_link_libraries(af_communication_interface INTERFACE
    af_models
)

# Create library for communication factory
add_library(af_communication_factory
    src/communication_factory.cpp
)
target_include_directories(af_communication_factory
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/..    # Add parent directory for proper include resolution
)
target_link_libraries(af_communication_factory
    PUBLIC
        af_communication_interface
    PRIVATE
        af_cpp_utils
)

# Add subdirectories for implementations
add_subdirectory(implementations/direct)
add_subdirectory(implementations/grpc)

# Create combined library that depends on available implementations
add_library(af_communication INTERFACE)
target_link_libraries(af_communication INTERFACE
    af_communication_interface
    af_communication_factory
    af_direct_communication
    af_grpc_communication
)

# Add gRPC implementation if available
if(TARGET af_grpc_communication AND NOT af_grpc_communication_INTERFACE_ONLY)
    target_link_libraries(af_communication INTERFACE af_grpc_communication)
endif()

# Install
install(TARGETS 
    af_communication_interface
    af_communication_factory
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/af/communication
)