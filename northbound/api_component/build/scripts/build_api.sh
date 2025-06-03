#!/bin/bash
# build_all.sh - Build script for the AF microservice application
#
# This script handles the build process for the entire AF microservice.
# It creates build directories, configures CMake, and builds all components.
#
# Usage:
#   ./build_all.sh [options]
#
# Options:
#   --clean        Clean build directory before building
#   --debug        Build in debug mode
#   --tests        Build tests
#   --install      Install after building
#   --deps         Install dependencies
#   --force-deps   Force reinstallation of dependencies
#   --help         Show this help message

# Exit on error
set -e

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-output"

# Source the build helper
source ${SCRIPT_DIR}/build_helper.af

# Default options
CLEAN=0
BUILD_TYPE="Release"
BUILD_TESTS=0
INSTALL=0
INSTALL_DEPS=0
FORCE_DEPS=0
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Parse command line arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --clean)
      CLEAN=1
      shift
      ;;
    --debug)
      BUILD_TYPE="Debug"
      shift
      ;;
    --tests)
      BUILD_TESTS=1
      shift
      ;;
    --install)
      INSTALL=1
      shift
      ;;
    --deps)
      INSTALL_DEPS=1
      shift
      ;;
    --force-deps)
      INSTALL_DEPS=1
      FORCE_DEPS=1
      shift
      ;;
    --help)
      echo "Usage: $0 [options]"
      echo "Options:"
      echo "  --clean        Clean build directory before building"
      echo "  --debug        Build in debug mode"
      echo "  --tests        Build tests"
      echo "  --install      Install after building"
      echo "  --deps         Install dependencies"
      echo "  --force-deps   Force reinstallation of dependencies"
      echo "  --help         Show this help message"
      exit 0
      ;;
    *)
      echo "Unknown option: $1"
      echo "Use --help for usage information"
      exit 1
      ;;
  esac
done

# Print build configuration
echo "Building AF microservice with configuration:"
echo "  Project root: ${PROJECT_ROOT}"
echo "  Build directory: ${BUILD_DIR}"
echo "  Build type: ${BUILD_TYPE}"
echo "  Clean build: ${CLEAN}"
echo "  Build tests: ${BUILD_TESTS}"
echo "  Install after build: ${INSTALL}"
echo "  Install dependencies: ${INSTALL_DEPS}"
echo "  Force reinstall dependencies: ${FORCE_DEPS}"
echo "  Parallel jobs: ${JOBS}"

# Install dependencies if requested
if [ $INSTALL_DEPS -eq 1 ]; then
  echo "Installing dependencies..."
  check_install_af_deps $FORCE_DEPS 0
  if [ $? -ne 0 ]; then
    echo "Failed to install dependencies"
    exit 1
  fi
fi


# Clean build directory if requested
if [ $CLEAN -eq 1 ] && [ -d "${BUILD_DIR}" ]; then
  echo "Cleaning build directory..."
  rm -rf "${BUILD_DIR}"
fi

# Create build directory if it doesn't exist
if [ ! -d "${BUILD_DIR}" ]; then
  echo "Creating build directory..."
  mkdir -p "${BUILD_DIR}"
fi

# Generate Protocol Buffer code
echo "Generating Protocol Buffer code..."
generate_proto_code $CLEAN

# Enter build directory
cd "${BUILD_DIR}"

# Configure CMake
echo "Configuring CMake..."
cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DBUILD_TESTS=$([[ $BUILD_TESTS -eq 1 ]] && echo "ON" || echo "OFF") \
      "${PROJECT_ROOT}"

# Build
echo "Building..."
cmake --build . -- -j${JOBS}

# Install if requested
if [ $INSTALL -eq 1 ]; then
  echo "Installing..."
  cmake --install .
fi

echo "Build completed successfully!"