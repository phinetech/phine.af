# Multi-stage build for AF Core Integration Tests
FROM ubuntu:22.04 AS builder

# Avoid interactive prompts during build
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    pkg-config \
    libssl-dev \
    zlib1g-dev \
    autoconf \
    libtool \
    curl \
    ca-certificates \
    libyaml-cpp-dev \
    libspdlog-dev \
    && rm -rf /var/lib/apt/lists/*

# Install gRPC and protobuf
WORKDIR /tmp
RUN git clone --recurse-submodules -b v1.58.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc && \
    cd grpc && \
    mkdir -p cmake/build && \
    cd cmake/build && \
    cmake -DgRPC_INSTALL=ON \
          -DgRPC_BUILD_TESTS=OFF \
          -DCMAKE_INSTALL_PREFIX=/usr/local \
          -DCMAKE_BUILD_TYPE=Release \
          ../.. && \
    make -j$(nproc) && \
    make install && \
    ldconfig && \
    cd /tmp && rm -rf grpc

# Install nlohmann_json
RUN git clone https://github.com/nlohmann/json.git && \
    cd json && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && rm -rf json

# Install Google Test
RUN git clone https://github.com/google/googletest.git -b v1.14.0 && \
    cd googletest && \
    mkdir build && cd build && \
    cmake .. -DBUILD_GMOCK=ON -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && rm -rf googletest

# Set up workspace
WORKDIR /workspace

# Copy common library first and build it
COPY common/ common/

# Build and install common library with system gRPC
RUN cd common && \
    mkdir -p build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DUSE_SYSTEM_GRPC=ON \
             -DUSE_SYSTEM_PROTOBUF=ON \
             -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Copy integration test source
COPY fixtures/ fixtures/
COPY config/ config/
COPY scripts/ scripts/
COPY framework/ framework/
COPY suites/ suites/
COPY main.cpp .
COPY CMakeLists.txt .

# Build the integration tests
RUN mkdir -p build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DUSE_SYSTEM_GRPC=ON \
             -DUSE_SYSTEM_PROTOBUF=ON && \
    cmake --build . --target af_integration_tests -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    ca-certificates \
    curl \
    netcat-traditional \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user for running tests
RUN useradd -m -u 1000 -s /bin/bash testuser && \
    mkdir -p /app/results && \
    chown -R testuser:testuser /app

WORKDIR /app

# Copy built test executable and dependencies
COPY --from=builder /workspace/build/af_integration_tests /app/
COPY --from=builder /usr/local/lib /usr/local/lib
COPY --from=builder /usr/local/bin /usr/local/bin

# Copy test fixtures and configurations
COPY --from=builder /workspace/fixtures /app/fixtures
COPY --from=builder /workspace/config /app/config
COPY --from=builder /workspace/scripts /app/scripts

# Make scripts executable
RUN chmod +x /app/scripts/*.sh 2>/dev/null || true

# Update library cache
RUN ldconfig

# Switch to non-root user
USER testuser

# Set environment variables
ENV GRPC_HOST=af_core
ENV GRPC_PORT=50051
ENV FIXTURES_PATH=/app/fixtures
ENV RESULTS_PATH=/app/results
ENV GTEST_COLOR=yes

# Default command - run all tests
CMD ["/app/af_integration_tests", \
     "--gtest_output=xml:/app/results/test_results.xml", \
     "--gtest_color=yes"]