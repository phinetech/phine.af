# Build


## Build using the repository script

Native CMake build of every C++ component in this repo (uses the same
toolchain as CI):

```bash
chmod +x build/scripts/ci_helper.sh
./build/scripts/ci_helper.sh build_standalone
```

### Common options

- `--clean`: clean build directory before building
- `--deps`: install dependencies
- `--debug`: build in debug mode
- `--tests`: build tests
- `--install`: install after building
- `--help`: show help

## Build container images

To build the deployable Docker images instead, use `docker build` directly:

```bash
# Bundled AF (AF Core + PCF handler in one image)
docker build -f Dockerfile -t phine.af .

# AF Core (microservice mode)
docker build -f af_core/Dockerfile -t phine.af-core .

# Southbound PCF handler (microservice mode)
docker build -f southbound/pcf_handler/Dockerfile -t phine.af-pcf-handler .

# Northbound HTTP API
docker build -f northbound/api_component/Dockerfile -t phine.af-api .
```

## Manual build using CMake

```bash
mkdir -p build-output
cd build-output
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -- -j$(nproc)
ctest
cmake --install .
```

<!---
TODO: Document which dependencies are installed by `--deps`.
-->
