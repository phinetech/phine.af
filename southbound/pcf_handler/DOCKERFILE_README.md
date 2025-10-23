# PCF Handler Docker Build - Multi-Stage Architecture

This Dockerfile uses a multi-stage build that separates the OAI CN5G common libraries from the PCF Handler application for better build efficiency and layer caching.

## Build Stages

### Stage 1: `base`
- Base Debian Bookworm slim image
- Sets common environment variables
- Serves as foundation for all other stages

### Stage 2: `dependencies`
- Builds all third-party dependencies from source:
  - gRPC v1.58.0 (with Protobuf)
  - nlohmann_json
  - spdlog
  - yaml-cpp v0.7.0
  - fmt v9.0.0
  - nghttp2 v1.65.0
- All dependencies are installed to `/usr/local/`
- This stage can be cached for fast rebuilds

### Stage 3: `oai-builder` ⭐ NEW
- **Dedicated stage for building OAI CN5G common libraries**
- Builds the following libraries:
  - `CONFIG` - Configuration management
  - `PCF` - PCF model classes
  - `COMMON_MODEL` - Common 3GPP models
  - `LOGGER` - Logging utilities
  - `NAS` - NAS protocol implementation
  - `COMMON` - Common utilities
- Libraries are built as static archives (`.a` files)
- Installs to `/usr/local/lib/` with headers in `/usr/local/include/oai`
- Creates CMake package config for easy discovery

### Stage 4: `common-runtime`
- Copies OAI libraries from `oai-builder` stage
- Builds AF common components (communication, components, DI, etc.)
- These are application-specific libraries that depend on OAI libraries

### Stage 5: `builder`
- **Uses pre-built OAI libraries from `oai-builder` stage**
- Copies OAI static libraries and headers
- Copies AF common libraries from `common-runtime` stage
- Builds PCF Handler application:
  - `libpcf_handler.a` - PCF Handler library
  - `pcf_server` - Main executable
- Uses `CMakeLists.txt.new` which expects OAI libraries to be pre-installed

### Stage 6: `runtime`
- Minimal runtime image with only necessary components
- Copies runtime dependencies (shared libraries)
- Copies static OAI libraries (embedded in executable)
- Copies `pcf_server` executable
- Runs as non-root user `afpcfuser`
- Exposes port 8080

## Build Flow

```
┌─────────────┐
│    base     │
└──────┬──────┘
       │
       ├─────────────────────────────────┐
       │                                 │
       ▼                                 ▼
┌─────────────┐                   ┌──────────────┐
│dependencies │                   │ oai-builder  │ Build OAI libs
└──────┬──────┘                   └──────┬───────┘
       │                                 │
       │         ┌───────────────────────┘
       │         │
       ▼         ▼
┌──────────────────────┐
│   common-runtime     │ (Build AF common libs)
└──────┬───────────────┘
       │
       │         ┌───────────────────────┐
       │         │  (OAI libs copied)    │
       ▼         ▼                       │
┌──────────────────────┐                │
│      builder         │ ◄──────────────┘
└──────┬───────────────┘
       │
       ▼
┌─────────────┐
│   runtime   │
└─────────────┘
```

## Benefits of Separate OAI Build Stage

### **Better Layer Caching**
- OAI libraries rarely change
- Changes to PCF Handler don't require rebuilding OAI libraries
- Docker can cache the `oai-builder` stage

### **Clear Dependency Separation**
- OAI common code isolated from application code
- Easier to understand build dependencies
- Better for debugging build issues

### **Parallel Building**
- Docker can build `oai-builder` and `dependencies` stages in parallel (if using BuildKit)
- More efficient use of CI/CD resources

### **Smaller Context**
- Only necessary files copied to each stage
- Reduced Docker build context size

## Building the Docker Image

### Standard Build
```bash
phine-cn5g-afs$ docker build -f southbound/pcf_handler/Dockerfile pcf-handler:latest .
```

## Running the Container

### Basic Run
```bash
docker run -p 8080:8080 pcf-handler:latest
```

### With Custom Config
```bash
docker run -p 8080:8080 \
  -v $(pwd)/config/pcf_handler.yaml:/etc/oai/af/pcf_handler.yaml \
  pcf-handler:latest
```

## References

- [Docker Multi-Stage Builds](https://docs.docker.com/build/building/multi-stage/)
- [BuildKit](https://docs.docker.com/build/buildkit/)
- [OAI CN5G Common Libraries](../BUILD_SEPARATION_README.md)
