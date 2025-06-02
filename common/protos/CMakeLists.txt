# CMakeLists.txt for Protobuf Definitions
#
# This file configures the build for protobuf message and service definitions
# used for internal AF communication.

# Find protobuf compiler and libraries
find_package(Protobuf REQUIRED)
find_package(gRPC QUIET)

# Get all proto files
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/*.proto")

# Set output directories
set(PROTO_SRC_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PROTO_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Make sure the output directory exists
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})

# Generate C++ code from proto files
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Create library for protobuf generated code
add_library(af_protos ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(af_protos PUBLIC protobuf::libprotobuf)
target_include_directories(af_protos PUBLIC
    ${PROTO_INCLUDE_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

# Install generated headers
install(DIRECTORY ${PROTO_INCLUDE_DIR}/
    DESTINATION include/af/protos
    FILES_MATCHING PATTERN "*.h"
)