# Build


## Build using the repository script

```bash
chmod +x build/scripts/build_all.sh
./build/scripts/build_all.sh
```

### Common options

- `--clean`: clean build directory before building
- `--deps`: install dependencies
- `--debug`: build in debug mode
- `--tests`: build tests
- `--install`: install after building
- `--help`: show help

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
The script delegates to `build/scripts/build_helper.af` which should be inspected when we document dependency installation.
Source: [build/scripts/build_all.sh](../../build/scripts/build_all.sh).
-->
