FROM phinetech/grpc-builder:v1.72.2 AS grpc-builder

# Base image with common settings
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
      git \
      libspdlog-dev \
      libyaml-cpp-dev \
      libfmt-dev \
      nlohmann-json3-dev \
  && rm -rf /var/lib/apt/lists/*


# Install gRPC and protobuf
WORKDIR /tmp

# Ensure packages are properly installed
RUN ldconfig

# Common runtime libraries stage
FROM base AS common-runtime
WORKDIR /app

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

# Ensure all libraries are registered
RUN ldconfig

# Install build dependencies
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      build-essential \
      libssl-dev \
      cmake \
      git \
      libboost-all-dev \
  && rm -rf /var/lib/apt/lists/*

# Build common runtime libraries
COPY common/ /app/common/
COPY build/cmake/ /app/build/cmake/
COPY CMakeLists.txt /app/CMakeLists.txt

RUN cd /app && \
    mkdir -p build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DUSE_SYSTEM_GRPC=ON \
             -DUSE_SYSTEM_PROTOBUF=ON \
             -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) && \
    make install && \
    ldconfig


# Builder stage - build the API component
FROM base AS builder
WORKDIR /app

# Copy the libraries built in the builder stage
# Copy libraries from previous stages
COPY --from=grpc-builder /usr/local/ /usr/local/
COPY --from=dependencies /usr/local/ /usr/local/
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libfmt.so /usr/lib/x86_64-linux-gnu/libfmt.so
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libspdlog.so /usr/lib/x86_64-linux-gnu/libspdlog.so
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libyaml-cpp.so /usr/lib/x86_64-linux-gnu/libyaml-cpp.so

# Copy headers/libs installed by dependencies stage
COPY --from=dependencies /usr/include/nlohmann /usr/include/nlohmann
COPY --from=dependencies /usr/include/fmt /usr/include/fmt
COPY --from=dependencies /usr/include/spdlog /usr/include/spdlog
COPY --from=dependencies /usr/include/yaml-cpp /usr/include/yaml-cpp

COPY --from=common-runtime /app/build/lib/ /usr/local/lib/
# Copy AF common headers
COPY --from=common-runtime /usr/local/include/af /usr/local/include/af
COPY --from=common-runtime /usr/local/include/oai/af /usr/local/include/oai/af
# Copy generated headers from common-runtime stage
COPY --from=common-runtime /app/build/generated/ /app/build-output/generated/


# Ensure libraries are properly linked
RUN ldconfig

# Install minimal build dependencies
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      build-essential \
      cmake \
      libssl-dev \
      libboost-all-dev \
      libc6-dev \
      linux-libc-dev \
      git \
      libspdlog-dev \
      libyaml-cpp-dev \
      libfmt-dev \
      nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*


RUN ls /app/build-output/generated/ || echo "Generated headers not found"

# Copy from root - parent CMakeLists will orchestrate build
COPY common/ /app/common/
COPY build/cmake/ /app/build/cmake/
COPY CMakeLists.txt /app/CMakeLists.txt
COPY af_core/ /app/af_core/

# # Workaround: Sync ALL missing files from af_core/common to root common to resolve split-brain includes
# # But exclude cmake directory to avoid conflicting FindProtobuf.cmake that causes target redefinition
# RUN cp -r /app/af_core/common/models /app/common/ 2>/dev/null || : && \
#     cp -r /app/af_core/common/communication /app/common/ 2>/dev/null || :

# Build the af_core component from root
RUN cd /app && \
    mkdir -p build-output && \
    cd build-output && \
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DUSE_SYSTEM_GRPC=ON \
             -DUSE_SYSTEM_PROTOBUF=ON \
             -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) af_core && \
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

# Copy system libraries (fmt is from Debian packages)
COPY --from=builder /usr/lib/x86_64-linux-gnu/libfmt.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libspdlog.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libyaml-cpp.so* /usr/lib/x86_64-linux-gnu/

# Copy only necessary runtime libraries
COPY --from=builder /app/build-output/lib/libaf_communication_factory* /usr/local/lib/
COPY --from=builder /app/build-output/lib/libaf_direct_communication* /usr/local/lib/
COPY --from=builder /app/build-output/lib/libaf_grpc_communication* /usr/local/lib/
COPY --from=builder /app/build-output/lib/libaf_common_component* /usr/local/lib/

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

RUN ldconfig

# Copy build artifacts
COPY --from=builder /app/build-output/bin/af_core /usr/local/bin/

COPY af_core/config/af_core.yaml /etc/oai/af/af_core.yaml

# Create a non-root user
RUN groupadd -r apiuser && useradd -r -g apiuser apiuser && \
    chown -R apiuser:apiuser /usr/local/bin/af_core /etc/oai/af

# Set working directory
WORKDIR /usr/local/bin

# Set permissions
RUN chmod +x ./af_core

# Switch to non-root user
USER apiuser

# Expose API port
EXPOSE 8080

# Command to run
CMD ["/usr/local/bin/af_core", "--config", "/etc/oai/af/af_core.yaml"]