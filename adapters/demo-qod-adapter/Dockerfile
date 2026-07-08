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
    cmake .. -DCMAKE_BUILD_TYPE=Release \
             -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
             -DBUILD_TESTING=OFF && \
    make -j$(nproc)

# ─────────────────────────────────────────────────────────────────────────
# Static analysis (clang-tidy) — not part of the default build.
# Build explicitly with: docker build --target static-analysis ...
# Run against a diff on stdin: docker run --rm -i <image> < component.diff
# ─────────────────────────────────────────────────────────────────────────
FROM builder AS static-analysis

RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      wget gnupg lsb-release software-properties-common ca-certificates python3 \
      libgtest-dev libgmock-dev \
    && wget -qO- https://apt.llvm.org/llvm.sh | bash -s -- 18 \
    && apt-get install --yes --no-install-recommends clang-tidy-18 \
    && rm -rf /var/lib/apt/lists/*

# Reconfigure (no need to build) so compile_commands.json includes the
# tests target — the main builder configure above uses
# -DBUILD_TESTING=OFF, so without this, clang-tidy has no compile-command
# entry at all for test source files and can't resolve <gtest/gtest.h>.
RUN cd /app/adapters/demo-qod-adapter/build && cmake -DBUILD_TESTING=ON .

COPY .clang-tidy /app/.clang-tidy
COPY .github/scripts/check_tidy.sh /usr/local/bin/check_tidy.sh
RUN chmod +x /usr/local/bin/check_tidy.sh

WORKDIR /app
ENTRYPOINT ["check_tidy.sh", "--build-dir", "adapters/demo-qod-adapter/build"]

# ─────────────────────────────────────────────────────────────────────────
# Unit tests (ctest) — not part of the default build.
# Build explicitly with: docker build --target tests ...
# ─────────────────────────────────────────────────────────────────────────
FROM builder AS tests

RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      libgtest-dev libgmock-dev \
    && rm -rf /var/lib/apt/lists/*

RUN cd /app/adapters/demo-qod-adapter/build && \
    cmake -DBUILD_TESTING=ON . && \
    make -j$(nproc) adapter_tests && \
    ctest -L unit --output-on-failure --output-junit results.xml

# ─────────────────────────────────────────────────────────────────────────
# Coverage (gcov via gcovr) — not part of the default build.
# Build explicitly with: docker build --target coverage ...
# ─────────────────────────────────────────────────────────────────────────
FROM builder AS coverage

RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
      libgtest-dev libgmock-dev python3-pip \
    && pip install --break-system-packages gcovr==8.6 \
    && rm -rf /var/lib/apt/lists/*

RUN cd /app/adapters/demo-qod-adapter/build && \
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DENABLE_COVERAGE=ON . && \
    make -j$(nproc) adapter_tests && \
    ctest -L unit --output-on-failure && \
    gcovr --root /app/adapters/demo-qod-adapter \
          --exclude '/app/adapters/demo-qod-adapter/tests/.*' \
          --exclude '/app/adapters/demo-qod-adapter/build/generated/.*' \
          --exclude '.*/nlohmann/.*' \
          --print-summary \
          --html-details coverage.html \
          .

# TODO: coverage is report-only for now (see docs/contributing.md#coverage)
# — the adapter's suite only covers QodClient/SessionManager, not the HTTP
# transport code, so a hard threshold would block unrelated PRs. Re-enable
# gating once coverage here is more established by uncommenting below
# (adjust thresholds per current coverage; gcovr fails the build, and
# therefore the CI job, if any threshold isn't met):
# RUN cd /app/adapters/demo-qod-adapter/build && \
#     gcovr --root /app/adapters/demo-qod-adapter \
#           --exclude '/app/adapters/demo-qod-adapter/tests/.*' \
#           --exclude '/app/adapters/demo-qod-adapter/build/generated/.*' \
#           --exclude '.*/nlohmann/.*' \
#           --fail-under-line 50 \
#           --fail-under-function 50 \
#           --fail-under-branch 30 \
#           --print-summary \
#           .

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
