# Base image with common settings
FROM debian:bullseye-slim AS base

ENV DEBIAN_FRONTEND=noninteractive
ENV IS_DOCKERFILE=1
WORKDIR /app

# Dependencies stage - build all required libraries
FROM base AS dependencies
# Install dependencies
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get upgrade --yes && \
    DEBIAN_FRONTEND=noninteractive apt-get install --yes \
      build-essential \
      cmake \
      psmisc \
      libssl-dev \
      libboost-system-dev \
      libboost-thread-dev \
      libboost-dev \
      pkg-config \
      git \
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

# Install spdlog
RUN git clone https://github.com/gabime/spdlog.git && \
    cd spdlog && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local \
             -DSPDLOG_BUILD_EXAMPLE=OFF \
             -DSPDLOG_BUILD_TESTS=OFF \
             -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
             -DSPDLOG_BUILD_SHARED=ON && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && rm -rf spdlog

# Install yaml-cpp
RUN git clone https://github.com/jbeder/yaml-cpp.git && \
    cd yaml-cpp && \
    git checkout yaml-cpp-0.7.0 && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DYAML_BUILD_TESTS=OFF -DYAML_BUILD_SHARED_LIBS=ON && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && rm -rf yaml-cpp

# Install nghttp2 with ASIO support
# Note: ENABLE_LIB_ONLY=OFF is required to install ASIO headers
# Boost and OpenSSL must be installed first for ASIO library to build
RUN git clone --recurse-submodules -b v1.65.0 --depth 1 --shallow-submodules https://github.com/nghttp2/nghttp2.git && \
    cd nghttp2 && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local \
             -DOPENSSL_ROOT_DIR=/usr \
             -DENABLE_LIB_ONLY=ON \
             -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && rm -rf nghttp2

# Ensure packages are properly installed
RUN ldconfig

# Create symlinks for nghttp2 libraries
RUN ln -sf /usr/local/lib/libnghttp2.so /usr/lib/libnghttp2.so

# Install nghttp2_asio
RUN git clone https://github.com/nghttp2/nghttp2-asio.git && \
    cd nghttp2-asio && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local \
             -DOPENSSL_ROOT_DIR=/usr \
             -DNGHTTP2_ROOT_DIR=/usr/local \
             -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install && \
    cd /tmp && rm -rf nghttp2-asio

RUN ldconfig

# Create symlinks for nghttp2_asio libraries
RUN ln -sf /usr/local/lib/libnghttp2_asio.so /usr/lib/libnghttp2_asio.so

# Common runtime libraries stage
FROM base AS common-runtime
WORKDIR /app

COPY --from=dependencies /usr/local/ /usr/local/

# Install build dependencies
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      build-essential \
      libssl-dev \
      cmake \
      git \
      libboost-system-dev \
      libboost-thread-dev \
      libboost-all-dev \
  && rm -rf /var/lib/apt/lists/*

# Build common runtime libraries
COPY ./common/ /app/common/

RUN cd /app/common && \
    mkdir -p build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DUSE_SYSTEM_GRPC=ON \
             -DUSE_SYSTEM_PROTOBUF=ON \
             -DUSE_SYSTEM_NGHTTP2=ON \
             -DUSE_SYSTEM_NGHTTP2_ASIO=ON \
             -DUSE_SYSTEM_OPENSSL=ON \
             -DUSE_SYSTEM_BOOST=ON \
             -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Builder stage - build the API component
FROM base AS builder
WORKDIR /app

# Copy the libraries built in the builder stage
# Copy libraries from previous stages
COPY --from=dependencies /usr/local/ /usr/local/
COPY --from=common-runtime /app/common/build/lib/ /usr/local/lib/
# Copy generated headers from common-runtime stage
COPY --from=common-runtime /app/common/build/generated/ /app/build-output/generated/

# Ensure libraries are properly linked
RUN ldconfig

# Install minimal build dependencies
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      build-essential \
      cmake \
      libssl-dev \
      libboost-system-dev \
      libboost-thread-dev \
      libboost-all-dev \
      libc6-dev \
      linux-libc-dev \
    && rm -rf /var/lib/apt/lists/*

RUN ls /app/build-output/generated/ || echo "Generated headers not found"

COPY ./common/ /app/common/
# COPY ./common/build/cmake /app/build/cmake/
COPY ./oai-cn5g-common-src /app/oai-cn5g-common-src
COPY ./pcf_handler/src /app/src/
COPY ./pcf_handler/include /app/include/
COPY ./pcf_handler/CMakeLists.txt /app/CMakeLists.txt

# Build the af_core component
RUN cd /app && \
    mkdir -p build-output && \
    cd build-output && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DUSE_SYSTEM_GRPC=ON \
             -DUSE_SYSTEM_PROTOBUF=ON \
             -DUSE_SYSTEM_NGHTTP2=ON \
             -DUSE_SYSTEM_NGHTTP2_ASIO=ON \
             -DUSE_SYSTEM_OPENSSL=ON \
             -DUSE_SYSTEM_BOOST=ON \
             -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    ldconfig

# Final runtime stage
FROM base AS runtime

# Copy necessary runtime dependencies from the builder image
COPY --from=builder /usr/local/lib/libnghttp2* /usr/local/lib/
COPY --from=builder /usr/local/lib/libgrpc* /usr/local/lib/
COPY --from=builder /usr/local/lib/libprotobuf* /usr/local/lib/
COPY --from=builder /usr/local/lib/libspdlog* /usr/local/lib/
COPY --from=builder /usr/local/lib/libyaml-cpp* /usr/local/lib/
COPY --from=builder /usr/local/lib/libboost* /usr/local/lib/
COPY --from=builder /usr/local/lib/libssl* /usr/local/lib/
COPY --from=builder /usr/local/lib/libcrypto* /usr/local/lib/
COPY --from=builder /usr/local/lib/libfmt* /usr/local/lib/

# Copy only necessary runtime libraries
COPY --from=builder /app/build-output/lib/libaf_common* /usr/local/lib/
COPY --from=builder /app/build-output/lib/libpcf_handler.so* /usr/local/lib/

RUN ldconfig

# Copy build artifacts
COPY --from=builder /app/build-output/bin/pcf_server /usr/local/bin/

# Install minimal build dependencies
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      libssl3 \
    && rm -rf /var/lib/apt/lists/*

COPY ./pcf_handler/config/pcf_handler.yaml /etc/oai/af/pcf_handler.yaml

# Create a non-root user
RUN groupadd -r afpcfuser && useradd -r -g afpcfuser afpcfuser && \
    chown -R afpcfuser:afpcfuser /usr/local/bin/pcf_server /etc/oai/af

# Set working directory
WORKDIR /usr/local/bin

# Set permissions
RUN chmod +x ./pcf_server

# Switch to non-root user
USER afpcfuser

# Expose API port
EXPOSE 8080

# Command to run
CMD ["/usr/local/bin/pcf_server", "--config", "/etc/oai/af/pcf_handler.yaml"]