# CMakeLists.txt for gRPC Communication Implementation
#
# This file configures the build for the gRPC implementation of the
# communication service. It depends on the gRPC and protobuf libraries
# and includes the generated protobuf/gRPC code.

# Find required packages
find_package(gRPC QUIET)
find_package(Protobuf REQUIRED)

if(NOT gRPC_FOUND)
    message(STATUS "gRPC not found - gRPC communication implementation will not be built")
    # Create a dummy library to satisfy dependencies
    add_library(af_grpc_communication INTERFACE)
    target_link_libraries(af_grpc_communication INTERFACE af_communication_interface)
    return()
endif()

message(STATUS "Building gRPC communication implementation")

# Add position-independent code flag
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Create library
add_library(af_grpc_communication
    src/grpc_communication_service.cpp
)

# Echo the gRPC and Protobuf include directories
message(STATUS "------------------- CMAKE_BINARY_DIR include directories: ${CMAKE_BINARY_DIR}")

# Set include directories
target_include_directories(af_grpc_communication
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_BINARY_DIR}/generated
        ${Protobuf_INCLUDE_DIRS}
        ${gRPC_INCLUDE_DIRS}
)

# Link dependencies
target_link_libraries(af_grpc_communication
    PUBLIC
        af_communication_interface
    PRIVATE
        af_cpp_utils
        af_protos
        protobuf::libprotobuf
        gRPC::grpc++
)

# Install
install(TARGETS af_grpc_communication
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include/af/communication/implementations/grpc
)

target_sources(af_grpc_communication
    PRIVATE
        ${CMAKE_BINARY_DIR}/generated/message.pb.cc
        ${CMAKE_BINARY_DIR}/generated/message.grpc.pb.cc
)