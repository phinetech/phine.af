# Base image with common settings
FROM phinetech/grpc-builder:v1.72.2 AS grpc-builder

FROM phinetech/oai-cn5g-common-src:latest AS oai-builder

FROM debian:bookworm-slim AS base

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
      libspdlog-dev \
      libyaml-cpp-dev \
      libfmt-dev \
      nlohmann-json3-dev \
      autoconf \
      automake \
      libtool \
      curl \
      make \
      g++ \
      unzip \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

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

# Copy built dependencies from dependencies stage
COPY --from=grpc-builder /usr/local/ /usr/local/
COPY --from=dependencies /usr/local/ /usr/local/
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libfmt.so* /usr/lib/x86_64-linux-gnu/libfmt.so
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libspdlog.so* /usr/lib/x86_64-linux-gnu/libspdlog.so
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libyaml-cpp.so* /usr/lib/x86_64-linux-gnu/libyaml-cpp.so

# Copy headers/libs installed by dependencies stage
COPY --from=dependencies /usr/include/nlohmann /usr/include/nlohmann
COPY --from=dependencies /usr/include/fmt /usr/include/fmt
COPY --from=dependencies /usr/include/spdlog /usr/include/spdlog
COPY --from=dependencies /usr/include/yaml-cpp /usr/include/yaml-cpp

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

RUN ldconfig

# Build common runtime libraries (AF common components)
COPY ./southbound/common/ /app/common/

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

# Builder stage - build the PCF Handler using pre-built OAI libraries
FROM base AS builder

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
      git \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy all dependencies and OAI libraries
COPY --from=grpc-builder /usr/local/ /usr/local/
COPY --from=dependencies /usr/local/ /usr/local/
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libfmt.so /usr/lib/x86_64-linux-gnu/libfmt.so
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libspdlog.so /usr/lib/x86_64-linux-gnu/libspdlog.so
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libyaml-cpp.so /usr/lib/x86_64-linux-gnu/libyaml-cpp.so

COPY --from=oai-builder /usr/local/lib/libCONFIG.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libPCF.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libCOMMON_MODEL.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libLOGGER.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libNAS.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libCOMMON.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libUTILS.a /usr/local/lib/
COPY --from=oai-builder /usr/local/include/oai /usr/local/include/oai
COPY --from=oai-builder /usr/local/lib/cmake/oai_cn5g_common /usr/local/lib/cmake/oai_cn5g_common

# Copy AF common libraries
COPY --from=common-runtime /app/common/build/lib/ /usr/local/lib/
# Copy AF common headers
COPY --from=common-runtime /usr/local/include/af /usr/local/include/af
COPY --from=common-runtime /usr/local/include/oai/af /usr/local/include/oai/af
# Copy headers of the dependencies installed in dependencies stage
COPY --from=dependencies /usr/include/nlohmann /usr/include/nlohmann
COPY --from=dependencies /usr/include/fmt /usr/include/fmt
COPY --from=dependencies /usr/include/spdlog /usr/include/spdlog
COPY --from=dependencies /usr/include/yaml-cpp /usr/include/yaml-cpp

# Copy headers for gRPC and protobuf
COPY --from=dependencies /usr/local/include/ /usr/local/include/

# Copy generated headers from common-runtime stage
COPY --from=common-runtime /app/common/build/generated/ /app/build-output/generated/

# Ensure libraries are properly linked
RUN ldconfig

RUN ls /app/build-output/generated/ || echo "Generated headers not found"

# Copy AF common source (needed for headers/includes)
COPY ./southbound/common/ /app/common/

# Copy PCF Handler source files
COPY ./southbound/pcf_handler/src /app/src/
COPY ./southbound/pcf_handler/build/cmake /app/build/cmake/
COPY ./southbound/pcf_handler/include /app/include/
COPY ./southbound/pcf_handler/CMakeLists.txt /app/CMakeLists.txt

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
COPY --from=builder /usr/local/lib/libgpr* /usr/local/lib/
COPY --from=builder /usr/local/lib/libprotobuf* /usr/local/lib/
COPY --from=builder /usr/local/lib/libspdlog* /usr/local/lib/
COPY --from=builder /usr/local/lib/libyaml-cpp* /usr/local/lib/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libboost* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/local/lib/libssl* /usr/local/lib/
COPY --from=builder /usr/local/lib/libcrypto* /usr/local/lib/
COPY --from=builder /usr/local/lib/libfmt* /usr/local/lib/
# COPY the gRPC dependencies built in the grpc-builder stage
COPY --from=grpc-builder /usr/local/lib/libgrpc* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libgpr* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libupb* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libre2* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libaddress_sorting* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libz.so* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libutf8* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libabsl* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libprotobuf* /usr/local/lib/

# Copy system libraries from builder
COPY --from=builder /usr/lib/x86_64-linux-gnu/libfmt.so /usr/lib/x86_64-linux-gnu/libfmt.so
COPY --from=builder /usr/lib/x86_64-linux-gnu/libspdlog.so /usr/lib/x86_64-linux-gnu/libspdlog.so
COPY --from=builder /usr/lib/x86_64-linux-gnu/libyaml-cpp.so /usr/lib/x86_64-linux-gnu/libyaml-cpp.so

# Copy OAI CN5G common libraries (static libraries, not needed at runtime but included for completeness)
COPY --from=oai-builder /usr/local/lib/libCONFIG.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libPCF.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libCOMMON_MODEL.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libLOGGER.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libNAS.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libCOMMON.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libUTILS.a /usr/local/lib/

# Copy AF common runtime libraries
COPY --from=builder /app/build-output/lib/libaf_common* /usr/local/lib/
COPY --from=builder /app/build-output/lib/libaf_communication_factory.so /usr/local/lib/
COPY --from=builder /app/build-output/lib/libaf_direct_communication.so /usr/local/lib/

RUN ldconfig

# Copy build artifacts
COPY --from=builder /app/build-output/bin/pcf_server /usr/local/bin/

# Install minimal build dependencies
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      libssl3 \
    && rm -rf /var/lib/apt/lists/*

COPY ./southbound/pcf_handler/config/pcf_handler.yaml /etc/oai/af/pcf_handler.yaml

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