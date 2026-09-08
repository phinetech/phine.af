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
    ca-certificates \
    cmake \
    curl \
    git \
    libnghttp2-dev \
    && rm -rf /var/lib/apt/lists/*

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
    -DBOOST_ROOT=/usr/local \
    -DBoost_NO_BOOST_CMAKE=ON \
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
# Copy Boost 1.83 headers and libs built from source in common-runtime
COPY --from=common-runtime /usr/local/include/boost /usr/local/include/boost
COPY --from=common-runtime /usr/local/lib/libboost_url.a /usr/local/lib/libboost_url.a
COPY --from=common-runtime /usr/local/lib/libboost_system.a /usr/local/lib/libboost_system.a
COPY --from=common-runtime /usr/local/lib/libboost_thread.a /usr/local/lib/libboost_thread.a
COPY --from=common-runtime /usr/local/lib/libboost_chrono.a /usr/local/lib/libboost_chrono.a
COPY --from=common-runtime /usr/local/lib/libboost_atomic.a /usr/local/lib/libboost_atomic.a


# Ensure libraries are properly linked
RUN ldconfig

# Install minimal build dependencies
RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
    build-essential \
    cmake \
    libssl-dev \
    libnghttp2-dev \
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
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DBUILD_TESTING=OFF \
    -DUSE_SYSTEM_GRPC=ON \
    -DUSE_SYSTEM_PROTOBUF=ON \
    -DBOOST_ROOT=/usr/local \
    -DBoost_NO_BOOST_CMAKE=ON \
    -DCMAKE_INSTALL_PREFIX=/usr/local && \
    make -j$(nproc) af_core && \
    make install && \
    ldconfig

# ─────────────────────────────────────────────────────────────────────────
# Checks (clang-tidy + unit tests) — not part of the default build.
# Build explicitly with: docker build --target checks ...
# Tests run at build time; the image still builds on test failure so CI can
# extract results.xml, and a test-exit-code file records the ctest outcome.
# Run clang-tidy against a diff on stdin: docker run --rm -i <image> < component.diff
# ─────────────────────────────────────────────────────────────────────────
FROM builder AS checks

RUN apt-get update && \
    apt-get install --yes --no-install-recommends \
    wget gnupg lsb-release software-properties-common ca-certificates python3 \
    libgtest-dev libgmock-dev \
    && wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | gpg --dearmor -o /etc/apt/trusted.gpg.d/apt.llvm.org.gpg \
    && echo "deb http://apt.llvm.org/bookworm/ llvm-toolchain-bookworm-18 main" > /etc/apt/sources.list.d/llvm-18.list \
    && apt-get update \
    && apt-get install --yes --no-install-recommends clang-tidy-18 \
    && rm -rf /var/lib/apt/lists/*

RUN cd /app/build-output && \
    cmake -DBUILD_TESTING=ON . && \
    make -j$(nproc) af_core_unit_tests && \
    { ctest -L unit --output-on-failure --output-junit results.xml; echo $? > test-exit-code; }

COPY .clang-tidy /app/.clang-tidy
COPY .github/scripts/check_tidy.sh /usr/local/bin/check_tidy.sh
RUN chmod +x /usr/local/bin/check_tidy.sh

WORKDIR /app
ENTRYPOINT ["check_tidy.sh", "--build-dir", "build-output"]

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

RUN cd /app/build-output && \
    cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DENABLE_COVERAGE=ON . && \
    make -j$(nproc) af_core_unit_tests && \
    ctest -L unit --output-on-failure && \
    gcovr --root /app/af_core \
    --exclude '.*/tests/.*' \
    --exclude '.*/generated/.*' \
    --print-summary \
    --html-details coverage.html \
    .

# TODO: coverage is report-only for now (see docs/contributing.md#coverage)
# — af_core's suite is still a light scaffold, so a hard threshold would
# block unrelated PRs. Re-enable gating once coverage here is more
# established by uncommenting below (adjust thresholds per current
# coverage; gcovr fails the build, and therefore the CI job, if any
# threshold isn't met):
# RUN cd /app/build-output && \
#     gcovr --root /app/af_core \
#           --exclude '.*/tests/.*' \
#           --exclude '.*/generated/.*' \
#           --fail-under-line 50 \
#           --fail-under-function 50 \
#           --fail-under-branch 30 \
#           --print-summary \
#           .

# Final runtime stage
FROM base AS runtime

# Copy necessary runtime dependencies from the builder image
COPY --from=builder /usr/lib/x86_64-linux-gnu/libnghttp2* /usr/lib/x86_64-linux-gnu/
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

# Debian-linked libs (Boost etc.) need libssl.so.3; grpc-builder ships libssl.so without the version suffix
RUN ln -sf /usr/local/lib/libssl.so /usr/local/lib/libssl.so.3 && \
    ln -sf /usr/local/lib/libcrypto.so /usr/local/lib/libcrypto.so.3 && \
    ldconfig

# Copy build artifacts
COPY --from=builder /app/build-output/bin/af_core /usr/local/bin/

COPY af_core/config/af_core.yaml /etc/phine.af/af_core.yaml

# Create a non-root user
RUN groupadd -r apiuser && useradd -r -g apiuser apiuser && \
    chown -R apiuser:apiuser /usr/local/bin/af_core /etc/phine.af

# Set working directory
WORKDIR /usr/local/bin

# Set permissions
RUN chmod +x ./af_core

# Switch to non-root user
USER apiuser

# Expose API port
EXPOSE 8080

# Command to run
CMD ["/usr/local/bin/af_core", "--config", "/etc/phine.af/af_core.yaml"]
