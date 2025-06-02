#!/bin/bash
# generate_grpc.sh - Generate C++ code from protobuf definitions
#
# This script simplifies the process of generating C++ code from protobuf and gRPC
# definitions. It can be used outside the CMake build process for development purposes.
#
# Usage:
#   ./generate_grpc.sh [proto_file] [output_dir]
#
# If no arguments are provided, it will generate code for all .proto files
# in the common/protos directory.

# Exit on error
set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
PROTO_DIR="${PROJECT_ROOT}/common/protos"
DEFAULT_OUTPUT_DIR="${PROJECT_ROOT}/build-output/generated"

# Check for protoc
PROTOC=$(which protoc 2>/dev/null || echo "")
if [ -z "${PROTOC}" ]; then
  echo "Error: protoc not found. Please install Protocol Buffers."
  exit 1
fi

# Check for grpc_cpp_plugin
GRPC_CPP_PLUGIN=$(which grpc_cpp_plugin 2>/dev/null || echo "")
if [ -z "${GRPC_CPP_PLUGIN}" ]; then
  echo "Warning: grpc_cpp_plugin not found. Will generate protobuf code only."
  GENERATE_GRPC=0
else
  GENERATE_GRPC=1
fi

# Parse arguments
if [ $# -ge 1 ]; then
  PROTO_FILES=("$1")
else
  PROTO_FILES=("${PROTO_DIR}"/*.proto)
fi

if [ $# -ge 2 ]; then
  OUTPUT_DIR="$2"
else
  OUTPUT_DIR="${DEFAULT_OUTPUT_DIR}"
fi

# Create output directory if it doesn't exist
mkdir -p "${OUTPUT_DIR}"

# Generate code for each proto file
for PROTO_FILE in "${PROTO_FILES[@]}"; do
  echo "Generating code from ${PROTO_FILE}..."
  
  if [ $GENERATE_GRPC -eq 1 ]; then
    ${PROTOC} --cpp_out="${OUTPUT_DIR}" \
              --grpc_out="${OUTPUT_DIR}" \
              --plugin=protoc-gen-grpc="${GRPC_CPP_PLUGIN}" \
              -I"$(dirname "${PROTO_FILE}")" \
              "${PROTO_FILE}"
  else
    ${PROTOC} --cpp_out="${OUTPUT_DIR}" \
              -I"$(dirname "${PROTO_FILE}")" \
              "${PROTO_FILE}"
  fi
done

echo "Code generation completed. Output in ${OUTPUT_DIR}"

value=$(jq -r '.end.sum_sent.bits_per_second / 1000000' /tmp/oai/qos-testing/iperf_result_ue-5qi-1.json);

if (( $(echo "$value >= 2.5 && $value <= 3.5" | bc -l) )); then
  # value is within the range
  echo "PASS: Max bitrate $value Mbps is within range (2.5-3.5)";
else
  # value is outside the range
  echo "FAIL: Max bitrate $value Mbps is outside the range (2.5-3.5)";
fi
