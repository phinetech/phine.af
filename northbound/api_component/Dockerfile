# Base image with common settings
FROM phinetech/grpc-builder:v1.72.2 AS grpc-builder

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
    ca-certificates \
    curl \
    make \
    g++ \
    unzip \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

# Install nghttp2 with ASIO support
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

# Build Boost 1.83 from source — apt provides only 1.74 on Debian Bookworm;
# boost::url (needed by af_http_communication) requires Boost >= 1.81.
# Static linking keeps the compiled Boost code inside the AF binaries so
# no extra .so files need to be copied into the runtime image.
RUN cd /tmp && \
    curl -fsSL https://archives.boost.io/release/1.83.0/source/boost_1_83_0.tar.gz \
    | tar -xz && \
    cd boost_1_83_0 && \
    ./bootstrap.sh --prefix=/usr/local \
    --with-libraries=url,system,thread && \
    ./b2 install -j$(nproc) \
    variant=release \
    link=static \
    threading=multi \
    cxxflags=-fPIC && \
    ldconfig && \
    cd /tmp && rm -rf boost_1_83_0
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
    libboost-system-dev \
    libboost-thread-dev \
    libboost-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

RUN ldconfig

# Copy cmake modules and common source
COPY ./common/cmake /app/common/cmake
COPY ./build/cmake /app/build/cmake

# Copy common source files
COPY ./common /app/common

# Build common libraries
WORKDIR /app/build-output
RUN cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_MODULE_PATH=/app/common/cmake:/app/build/cmake \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DBOOST_ROOT=/usr/local \
    -DBoost_NO_BOOST_CMAKE=ON \
    /app/common && \
    make -j$(nproc) && \
    make install

RUN ldconfig

# Builder stage - build the API component
FROM base AS builder
WORKDIR /app

# Copy libraries from previous stages
COPY --from=grpc-builder /usr/local/ /usr/local/
COPY --from=dependencies /usr/local/ /usr/local/
COPY --from=common-runtime /usr/local/ /usr/local/
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libfmt.so* /usr/lib/x86_64-linux-gnu/libfmt.so
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libspdlog.so* /usr/lib/x86_64-linux-gnu/libspdlog.so
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libyaml-cpp.so* /usr/lib/x86_64-linux-gnu/libyaml-cpp.so

# Copy headers/libs installed by dependencies stage
COPY --from=dependencies /usr/include/nlohmann /usr/include/nlohmann
COPY --from=dependencies /usr/include/fmt /usr/include/fmt
COPY --from=dependencies /usr/include/spdlog /usr/include/spdlog
COPY --from=dependencies /usr/include/yaml-cpp /usr/include/yaml-cpp

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
    libboost-dev \
    libspdlog-dev \
    libyaml-cpp-dev \
    libfmt-dev \
    nlohmann-json3-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Copy cmake modules
COPY ./common/cmake /app/common/cmake
COPY ./build/cmake /app/build/cmake

# Copy common headers (needed for include paths)
COPY ./common /app/common

# Copy api_component source files
COPY ./northbound/api_component/src /app/src
COPY ./northbound/api_component/include /app/include
COPY ./northbound/api_component/CMakeLists.txt /app/CMakeLists.txt

# Build api_component
WORKDIR /app/build-output
RUN cmake -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_MODULE_PATH=/app/common/cmake:/app/build/cmake \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    /app && \
    make -j$(nproc)

# Final runtime stage
FROM base AS runtime

# Install runtime dependencies
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
    libssl3 \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    && rm -rf /var/lib/apt/lists/*

# Copy necessary runtime dependencies from the builder image
COPY --from=builder /usr/local/lib/libssl.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libcrypto.so* /usr/local/lib/
COPY --from=builder /usr/local/lib/libnghttp2* /usr/local/lib/
COPY --from=builder /usr/local/lib/libgrpc* /usr/local/lib/
COPY --from=builder /usr/local/lib/libprotobuf* /usr/local/lib/
COPY --from=builder /usr/local/lib/libabsl* /usr/local/lib/
COPY --from=builder /usr/local/lib/libupb* /usr/local/lib/
COPY --from=builder /usr/local/lib/libaddress_sorting* /usr/local/lib/
COPY --from=builder /usr/local/lib/libre2* /usr/local/lib/
COPY --from=builder /usr/local/lib/libgpr* /usr/local/lib/
COPY --from=builder /usr/local/lib/libutf8* /usr/local/lib/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libspdlog.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libyaml-cpp.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libfmt.so* /usr/lib/x86_64-linux-gnu/

# Copy AF common libraries
COPY --from=builder /usr/local/lib/libaf_* /usr/local/lib/

# Copy api_component library and binary
COPY --from=builder /app/build-output/lib/libapi_component.so* /usr/local/lib/
COPY --from=builder /app/build-output/bin/api_server /usr/local/bin/

RUN ldconfig

# Copy configuration
COPY ./northbound/api_component/config/api_adapter.yaml /etc/oai/af/api_adapter.yaml

# Create a non-root user
RUN groupadd -r apiuser && useradd -r -g apiuser apiuser && \
    mkdir -p /etc/oai/af && \
    chown -R apiuser:apiuser /usr/local/bin/api_server /etc/oai/af

# Set working directory
WORKDIR /usr/local/bin

# Set permissions
RUN chmod +x ./api_server

# Switch to non-root user
USER apiuser

# Expose API port
EXPOSE 8080

# Command to run
CMD ["/usr/local/bin/api_server", "--config", "/etc/oai/af/api_adapter.yaml"]
