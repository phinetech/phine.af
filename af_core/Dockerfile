# Use the official Debian base image with C++ development tools
FROM ubuntu:22.04 as builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler-grpc \
    libyaml-cpp-dev \
    libspdlog-dev \
    nlohmann-json3-dev \
    pkg-config \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create a working directory
WORKDIR /app/build

# Copy the source code
COPY . /app/

# Build the application
RUN cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    && make -j$(nproc) \
    && make install

# Create the runtime image
FROM debian:bullseye-slim

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libgrpc++1 \
    libprotobuf23 \
    libyaml-cpp0.7 \
    libspdlog1 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Create configuration directory
RUN mkdir -p /etc/oai/af

# Copy the built executable and libraries
COPY --from=builder /usr/local/bin/af_core /usr/local/bin/
COPY --from=builder /usr/local/lib/*.so* /usr/local/lib/

# Copy default configuration
COPY --from=builder /app/config/af_core.yaml /etc/oai/af/

# Update library cache
RUN ldconfig

# Set the working directory
WORKDIR /app

# Set environment variables
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Create volume mount points
VOLUME ["/etc/oai/af", "/app/logs"]

# Expose the gRPC port
EXPOSE 50051

# Set the entrypoint
ENTRYPOINT ["/usr/local/bin/af_core", "--config", "/etc/oai/af/af_core.yaml"]