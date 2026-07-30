# Dockerfile for the bundled AF application
# Builds AF Core and the southbound PCF handler into a single binary.
# Northbound applications remain standalone services.

# Pre-built gRPC/protobuf
FROM phinetech/grpc-builder:v1.72.2 AS grpc-builder

# Pre-built OAI CN5G Common libraries (needed by southbound/pcf_handler)
FROM phinetech/oai-cn5g-common-src:latest AS oai-builder

# Base image
FROM debian:bookworm-slim AS base
ENV DEBIAN_FRONTEND=noninteractive
ENV IS_DOCKERFILE=1
# Allow pkg-config and CMake helpers to discover libraries and .pc files copied
# into /usr/local from earlier build stages, especially nghttp2/nghttp2_asio.
ENV PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/lib/pkgconfig
WORKDIR /app

# =============================================================================
# Dependencies stage - install system packages and build nghttp2
# =============================================================================
FROM base AS dependencies

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
    libboost-all-dev \
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
# Note: ENABLE_LIB_ONLY=ON builds the shared nghttp2 library required by nghttp2_asio.
# Boost and OpenSSL must be installed first for the ASIO library to build.
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

# Build Boost 1.83 from source — apt provides only 1.74 on Debian Bookworm;
# boost::url (needed by af_http_communication) requires Boost >= 1.81.
# Static linking keeps the compiled Boost code inside the AF binaries so
# no extra .so files need to be copied into the runtime image.
RUN cd /tmp && \
    curl -fsSL https://archives.boost.io/release/1.83.0/source/boost_1_83_0.tar.gz \
    | tar -xz && \
    cd boost_1_83_0 && \
    ./bootstrap.sh --prefix=/usr/local \
    --with-libraries=url,system,thread,chrono,atomic && \
    ./b2 install -j$(nproc) \
    variant=release \
    link=static \
    threading=multi \
    cxxflags=-fPIC && \
    ldconfig && \
    cd /tmp && rm -rf boost_1_83_0

# =============================================================================
# Builder stage - build the bundled AF application
# =============================================================================
FROM base AS builder

# Install build dependencies
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
    build-essential \
    cmake \
    libssl-dev \
    ca-certificates \
    libboost-system-dev \
    libboost-thread-dev \
    libboost-all-dev \
    libc6-dev \
    linux-libc-dev \
    git \
    pkg-config \
    libspdlog-dev \
    libyaml-cpp-dev \
    libfmt-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy pre-built libraries
COPY --from=grpc-builder /usr/local/ /usr/local/
COPY --from=dependencies /usr/local/ /usr/local/

# Copy system libraries from dependencies
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libfmt.so* /usr/lib/x86_64-linux-gnu/libfmt.so
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libspdlog.so* /usr/lib/x86_64-linux-gnu/libspdlog.so
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libyaml-cpp.so* /usr/lib/x86_64-linux-gnu/libyaml-cpp.so

# Copy headers from dependencies
COPY --from=dependencies /usr/include/nlohmann /usr/include/nlohmann
COPY --from=dependencies /usr/include/fmt /usr/include/fmt
COPY --from=dependencies /usr/include/spdlog /usr/include/spdlog
COPY --from=dependencies /usr/include/yaml-cpp /usr/include/yaml-cpp

# Copy OAI CN5G Common libraries (needed by southbound pcf_handler)
COPY --from=oai-builder /usr/local/lib/libCONFIG.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libPCF.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libCOMMON_MODEL.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libLOGGER.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libNAS.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libCOMMON.a /usr/local/lib/
COPY --from=oai-builder /usr/local/lib/libUTILS.a /usr/local/lib/
COPY --from=oai-builder /usr/local/include/oai /usr/local/include/oai
COPY --from=oai-builder /usr/local/lib/cmake/oai_cn5g_common /usr/local/lib/cmake/oai_cn5g_common

RUN ldconfig
RUN ln -sf /usr/local/lib/libnghttp2.so /usr/lib/libnghttp2.so && \
    ln -sf /usr/local/lib/libnghttp2_asio.so /usr/lib/libnghttp2_asio.so

# Copy entire project source
COPY common/ /app/common/
COPY build/cmake/ /app/build/cmake/
COPY CMakeLists.txt /app/CMakeLists.txt
COPY src/ /app/src/
COPY af_core/ /app/af_core/
COPY northbound/ /app/northbound/
COPY southbound/ /app/southbound/
COPY config/ /app/config/

# Compile-time handler selection
ARG ENABLE_PCF_HANDLER=ON
ARG ENABLE_NEF_HANDLER=OFF
ARG ENABLE_UDR_HANDLER=OFF

# Build the bundled application
RUN cd /app && \
    mkdir -p build-output && \
    cd build-output && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_BUNDLED=ON \
    -DENABLE_PCF_HANDLER=${ENABLE_PCF_HANDLER} \
    -DENABLE_NEF_HANDLER=${ENABLE_NEF_HANDLER} \
    -DENABLE_UDR_HANDLER=${ENABLE_UDR_HANDLER} \
    -DUSE_SYSTEM_GRPC=ON \
    -DUSE_SYSTEM_PROTOBUF=ON \
    -DUSE_SYSTEM_NGHTTP2=ON \
    -DUSE_SYSTEM_NGHTTP2_ASIO=ON \
    -DUSE_SYSTEM_OPENSSL=ON \
    -DUSE_SYSTEM_BOOST=ON \
    -DBOOST_ROOT=/usr/local \
    -DBoost_NO_BOOST_CMAKE=ON \
    -DCMAKE_PREFIX_PATH=/usr/local \
    -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) af && \
    make install && \
    ldconfig

# =============================================================================
# Runtime stage - minimal image with just the binary
# =============================================================================
FROM base AS runtime

# Copy runtime libraries from builder
COPY --from=builder /usr/local/lib/libnghttp2* /usr/local/lib/
COPY --from=builder /usr/local/lib/libgrpc* /usr/local/lib/
COPY --from=builder /usr/local/lib/libgpr* /usr/local/lib/
COPY --from=builder /usr/local/lib/libprotobuf* /usr/local/lib/
COPY --from=builder /usr/local/lib/libssl* /usr/local/lib/
COPY --from=builder /usr/local/lib/libcrypto* /usr/local/lib/

# Copy system libraries
COPY --from=builder /usr/lib/x86_64-linux-gnu/libfmt.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libspdlog.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libyaml-cpp.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libboost* /usr/lib/x86_64-linux-gnu/

# Copy AF common shared libraries
COPY --from=builder /app/build-output/lib/libaf_communication_factory* /usr/local/lib/
COPY --from=builder /app/build-output/lib/libaf_direct_communication* /usr/local/lib/
COPY --from=builder /app/build-output/lib/libaf_grpc_communication* /usr/local/lib/
COPY --from=builder /app/build-output/lib/libaf_common_component* /usr/local/lib/

# Copy gRPC runtime dependencies
COPY --from=grpc-builder /usr/local/lib/libgrpc* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libgpr* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libupb* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libre2* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libaddress_sorting* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libz.so* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libutf8* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libabsl* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libprotobuf* /usr/local/lib/

# Install minimal runtime dependency
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

RUN ldconfig

# Copy the bundled binary
COPY --from=builder /app/build-output/bin/af /usr/local/bin/

# Copy unified config
COPY config/af.yaml /etc/oai/af/af.yaml

# Create a non-root user
RUN groupadd -r afuser && useradd -r -g afuser afuser && \
    chown -R afuser:afuser /usr/local/bin/af /etc/oai/af

WORKDIR /usr/local/bin
RUN chmod +x ./af

USER afuser

# Expose ports: AF Core gRPC (50051)
EXPOSE 50051

CMD ["/usr/local/bin/af", "--config", "/etc/oai/af/af.yaml"]
