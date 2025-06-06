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
      git \
  && rm -rf /var/lib/apt/lists/*

  
COPY ./common/build/scripts /app/common/build/scripts/
COPY ./common/build/cmake/FindgRPC.cmake /app/common/build/cmake/FindgRPC.cmake
COPY ./common/build/cmake/Protobuf.cmake /app/common/build/cmake/Protobuf.cmake
COPY ./build/scripts /app/build/scripts/
# COPY ./build/cmake /app/build/cmake/

WORKDIR /app/build/scripts
RUN chmod +x build_core.sh && \
    ./build_core.sh --clean --deps-only --force-deps

# Ensure packages are properly installed
RUN ldconfig

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
      libboost-all-dev \
  && rm -rf /var/lib/apt/lists/*

# Copy the .proto files
COPY ./common /app/common
COPY ./common/build/cmake/FindgRPC.cmake /app/common/build/cmake/FindgRPC.cmake
COPY ./common/build/cmake/Protobuf.cmake /app/common/build/cmake/Protobuf.cmake
COPY ./build/scripts /app/build/scripts/
# COPY ./build/cmake /app/build/cmake/

WORKDIR /app/build/scripts
RUN chmod +x build_core.sh && \
    ./build_core.sh --clean --common-only


# Builder stage - build the API component
FROM base AS builder
WORKDIR /app

# Copy the libraries built in the builder stage
# Copy libraries from previous stages
COPY --from=dependencies /usr/local/ /usr/local/
COPY --from=common-runtime /app/build-output/lib/ /usr/local/lib/


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
    && rm -rf /var/lib/apt/lists/*

# Copy generated headers from common-runtime stage
COPY --from=common-runtime /app/build-output/generated/ /app/build-output/generated/

RUN ls /app/build-output/generated/ || echo "Generated headers not found"

COPY ./common/ /app/common/
COPY ./build/scripts /app/build/scripts/
# COPY ./build/cmake /app/build/cmake/
COPY ./src /app/src/
COPY ./include /app/include/
COPY ./CMakeLists.txt /app/CMakeLists.txt

WORKDIR /app/build/scripts
RUN chmod +x build_core.sh && \
    ./build_core.sh

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

# Copy only necessary runtime libraries
COPY --from=builder /app/build-output/lib/libaf_common* /usr/local/lib/
COPY --from=builder /app/build-output/lib/libaf_core_lib.so* /usr/local/lib/

RUN ldconfig

# Copy build artifacts
COPY --from=builder /app/build-output/bin/af_core /usr/local/bin/

COPY ./config/af_core.yaml /etc/oai/af/af_core.yaml

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