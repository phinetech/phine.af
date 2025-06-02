FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
ENV IS_DOCKERFILE=1

# Install dependencies
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get upgrade --yes && \
    DEBIAN_FRONTEND=noninteractive apt-get install --yes \
      build-essential \
      cmake \
      psmisc \
      git \
  && rm -rf /var/lib/apt/lists/*

  
WORKDIR /northbound-api-component
COPY ./build/scripts /northbound-api-component/build/scripts/
# COPY ./build/common-build /northbound-api-component/build/common-build/
COPY ./CMakeLists.txt /northbound-api-component/CMakeLists.txt
COPY ./build/cmake /northbound-api-component/build/cmake/

WORKDIR /northbound-api-component/build/scripts
RUN chmod +x build_api.sh
RUN ./build_api.sh --clean --deps


# Runtime image
FROM ubuntu:22.04

# Copy necessary runtime dependencies from the builder image
COPY --from=builder /usr/local/lib/libnghttp2* /usr/local/lib/
COPY --from=builder /usr/local/lib/libgrpc* /usr/local/lib/
COPY --from=builder /usr/local/lib/libprotobuf* /usr/local/lib/
COPY --from=builder /usr/local/lib/libspdlog* /usr/local/lib/
COPY --from=builder /usr/local/lib/libyaml-cpp* /usr/local/lib/
COPY --from=builder /usr/local/lib/libboost* /usr/local/lib/
COPY --from=builder /usr/local/lib/libssl* /usr/local/lib/
COPY --from=builder /usr/local/lib/libcrypto* /usr/local/lib/

RUN ldconfig

# Copy build artifacts
COPY --from=builder /app/build/northbound/api_component/api_server /usr/local/bin/
COPY --from=builder /app/config/api_adapter.yaml /etc/oai/af/api_adapter.yaml

# Set working directory
WORKDIR /usr/local/bin

# Expose API port
EXPOSE 8080

# Command to run
ENTRYPOINT ["/usr/local/bin/api_server", "--config", "/etc/oai/af/api_adapter.yaml"]