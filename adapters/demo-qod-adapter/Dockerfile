# ── Stage 1: Pre-built gRPC/Protobuf ─────────────────────────────────────────
FROM phinetech/grpc-builder:v1.72.2 AS grpc-builder

# ── Stage 2: Base image ──────────────────────────────────────────────────────
FROM debian:bookworm-slim AS base
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /app

# ── Stage 3: System dependencies ─────────────────────────────────────────────
FROM base AS dependencies
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      build-essential \
      cmake \
      git \
      libspdlog-dev \
      libyaml-cpp-dev \
      libfmt-dev \
      nlohmann-json3-dev \
      libssl-dev \
    && rm -rf /var/lib/apt/lists/*

RUN ldconfig

# ── Stage 4: Build ───────────────────────────────────────────────────────────
FROM base AS builder
WORKDIR /app

# Copy gRPC/Protobuf from pre-built image
COPY --from=grpc-builder /usr/local/ /usr/local/

# Copy system-installed headers/libs from dependencies stage
COPY --from=dependencies /usr/include/nlohmann    /usr/include/nlohmann
COPY --from=dependencies /usr/include/fmt         /usr/include/fmt
COPY --from=dependencies /usr/include/spdlog      /usr/include/spdlog
COPY --from=dependencies /usr/include/yaml-cpp    /usr/include/yaml-cpp
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libfmt.so*       /usr/lib/x86_64-linux-gnu/
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libspdlog.so*    /usr/lib/x86_64-linux-gnu/
COPY --from=dependencies /usr/lib/x86_64-linux-gnu/libyaml-cpp.so*  /usr/lib/x86_64-linux-gnu/

# Install minimal build tools
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      build-essential \
      cmake \
      libboost-system-dev \
      libssl-dev \
      libspdlog-dev \
      libyaml-cpp-dev \
      libfmt-dev \
      nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

RUN ldconfig

# Copy only what we need: shared proto/config support and the adapter source
COPY common/protos/message.proto /app/common/protos/message.proto
COPY common/config/ /app/common/config/
COPY adapters/demo-qod-adapter/  /app/adapters/demo-qod-adapter/

# Build the adapter
RUN cd /app/adapters/demo-qod-adapter && \
    mkdir -p build && \
    cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# ── Stage 5: Runtime ─────────────────────────────────────────────────────────
FROM base AS runtime

# Install runtime dependencies (ca-certificates for SSL)
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Copy runtime shared libraries
COPY --from=grpc-builder /usr/local/lib/libgrpc*     /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libgpr*      /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libupb*      /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libre2*      /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libaddress_sorting* /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libz.so*     /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libutf8*     /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libabsl*     /usr/local/lib/
COPY --from=grpc-builder /usr/local/lib/libprotobuf* /usr/local/lib/

COPY --from=builder /usr/lib/x86_64-linux-gnu/libfmt.so*       /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libspdlog.so*    /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libyaml-cpp.so*  /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libboost_system.so* /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libssl.so*       /usr/lib/x86_64-linux-gnu/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libcrypto.so*    /usr/lib/x86_64-linux-gnu/

RUN ldconfig

# Copy the built binary
COPY --from=builder /app/adapters/demo-qod-adapter/build/bin/demo_qod_adapter \
    /usr/local/bin/demo_qod_adapter

# Copy default config
COPY adapters/demo-qod-adapter/config.yaml /app/config.yaml

# Non-root user
RUN groupadd -r adapter && useradd -r -g adapter adapter && \
    chown -R adapter:adapter /app
USER adapter

WORKDIR /app

CMD ["/usr/local/bin/demo_qod_adapter", "/app/config.yaml"]
