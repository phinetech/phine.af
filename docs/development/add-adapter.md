# Add a new adapter

This page documents the *repository-derived* pattern for building a new adapter in phine.af.

An adapter can be any application that interfaces between the 5G domain and another domain. In practice, an adapter usually:

- accepts input from an external system, protocol, or configuration source
- translates that input into AF-facing requests
- sends those requests to phine.af over the internal AF interface
- optionally monitors and cleans up state over time

The demo QoD adapter is the current reference implementation. It shows a simple path from a YAML configuration file to CAMARA QoD requests sent to AF Core.

## What an adapter should do

A new adapter should keep three concerns separate:

1. **Domain-facing input**
   - files, events, messages, HTTP requests, ROS2 topics, or another application domain
2. **Adapter logic**
   - validation, mapping, retries, monitoring, lifecycle control
3. **AF communication**
   - sending internal AF requests and interpreting AF responses

This separation keeps the adapter easy to run in standalone mode and easy to bundle into a single deliverable later.

## Repository placement

Create a new adapter under `adapters/`.

Minimal layout:

```text
adapters/my-adapter/
├── CMakeLists.txt
├── Dockerfile
├── Dockerfile.bundle          # optional, if bundled shipping is supported
├── BundleInject.cmake         # optional, if bundled shipping is supported
├── README.md
├── config.yaml                # adapter-local configuration example
├── include/
├── src/
│   ├── main.cpp
│   ├── bundled_entry.cpp      # optional, if bundled shipping is supported
│   └── ...
└── tests/
```

The demo reference is in `adapters/demo-qod-adapter/`.

## Recommended target structure

Use two logical layers:

- **adapter library**
  - reusable logic, models, client code, lifecycle code
- **entrypoints**
  - standalone executable entrypoint
  - bundled entrypoint

For the demo adapter this becomes:

- `adapter_lib`
- `demo_qod_adapter`
- `demo_qod_adapter_bundle`

The adapter library should contain most of the real logic. The entrypoints should stay thin.

## Configuration pattern

For a new adapter, prefer reusing `af::config` from `common/config` for scalar configuration values that fit the shared AF configuration model.

Examples of values that should usually go through `af::config`:

- endpoint addresses
- ports
- timeouts
- retry settings
- logging settings
- simple mode flags

For adapter-specific structures that are strongly domain-shaped, native parsing in the adapter is still acceptable.

Examples:

- lists of streams
- nested routing rules
- event mappings
- domain-specific payload templates

The demo QoD adapter now follows this split:

- shared scalar loading through `af::config::Configuration`
- native YAML parsing for `streams[]` and related list-heavy structures

This keeps the adapter aligned with the common configuration handling used elsewhere in the repository, without forcing highly adapter-specific models into `common/config`.

## Standalone adapter pattern

Standalone mode is the default development path.

In this mode:

- the adapter builds from its own directory
- the adapter runs as its own process or container
- the adapter talks to AF Core over gRPC

Typical responsibilities of `main.cpp`:

- load adapter configuration
- create the AF client
- create the session or workflow manager
- run the adapter loop
- handle shutdown and cleanup

In the demo adapter, `main.cpp` delegates configuration loading to a shared helper instead of directly parsing all YAML fields itself.

This is the simplest way to develop a new integration.

## Bundled adapter pattern

Some adapters should also support a bundled deployment so a single container or appliance can be shipped.

In this repository, *bundled* means:

- AF Core and southbound handlers are packaged inside `af`
- the adapter is added into that same `af` process
- the bundle is still built from the root AF build
- the adapter owns its own bundle integration logic

This keeps the root AF build generic while allowing an adapter developer to decide whether bundling is supported.

## Bundling design used in this repository

The current repository pattern is:

1. the root build remains the top-level CMake project
2. the adapter provides a small `BundleInject.cmake`
3. the root configure is invoked with `CMAKE_PROJECT_<project>_INCLUDE`
4. the adapter adds itself into the root build with `add_subdirectory(...)`
5. the adapter links its bundle target into `af`

This is the key point: **the root `CMakeLists.txt` does not need adapter-specific wiring logic**.

## Why `BundleInject.cmake` exists

`BundleInject.cmake` does only one job:

- inject the adapter directory into the root build

It does **not** replace the adapter's own `CMakeLists.txt`.

The adapter `CMakeLists.txt` still owns:

- adapter targets
- bundle target definition
- delayed linking into `af` when target creation order requires it

This is why the demo adapter has both:

- `BundleInject.cmake`
- bundle wiring logic in `CMakeLists.txt`

## Minimal CMake checklist

For a new adapter, the `CMakeLists.txt` should usually provide:

- a top-level detection flag for standalone builds
- an option to build the standalone executable
- an option to enable bundle support
- a reusable adapter library target
- an optional bundled object or static library target

Repository-derived checklist:

- define `BUILD_<ADAPTER>_STANDALONE`
- define `BUILD_<ADAPTER>_BUNDLE`
- create `adapter_lib`
- create `<adapter>_bundle`
- when bundled, link `<adapter>_bundle` into `af`

If the adapter is injected before the root `af` target exists, defer the final link step until the root target has been created.

## Build flow for a bundled adapter

The bundled build is still started from the repository root.

Example flow:

```bash
cmake -S . -B build-my-adapter-bundle \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_BUNDLED=ON \
  -DCMAKE_PROJECT_phine.af_INCLUDE="$PWD/adapters/my-adapter/BundleInject.cmake"

cmake --build build-my-adapter-bundle --target af -j$(nproc)
```

This produces a bundled `af` executable that includes the adapter.

## What goes into `bundled_entry.cpp`

The bundled entrypoint should stay small.

Typical responsibilities:

- load bundled adapter configuration
- start adapter logic inside the process
- wait for AF readiness if the adapter depends on AF's gRPC endpoint
- perform monitoring and cleanup on shutdown

Avoid moving core domain logic into `bundled_entry.cpp`.

In the demo adapter, `bundled_entry.cpp` reuses the same shared configuration helper as standalone mode so that both modes interpret configuration consistently.

## Runtime model for bundled adapters

The current repository pattern keeps the adapter logic mostly unchanged.

That means a bundled adapter may still use the same AF gRPC client path as the standalone adapter. This is acceptable for a first bundled version because it minimizes refactoring.

Longer term, an adapter may replace the loopback gRPC path with an in-process AF client abstraction if lower overhead or tighter coupling is needed.

## Docker packaging

If an adapter supports standalone shipping, provide:

- `Dockerfile`

If an adapter supports bundled shipping, provide:

- `Dockerfile.bundle`

The bundled Dockerfile should:

- use the repository root as the build context
- invoke the root bundled AF build
- inject the adapter with `BundleInject.cmake`
- copy adapter-specific configuration into the runtime image

For the demo adapter, the bundled image also sets:

- `PHINE_DEMO_QOD_ADAPTER_CONFIG=/etc/oai/af/demo_qod_adapter.yaml`

## Compose packaging

If bundled deployment is intended to be easy to run, provide a dedicated Compose file.

The demo reference is:

- `docker-compose/docker-compose-demo-qod-bundled.yaml`

This keeps the adapter-specific shipping path separate from the generic bundled AF deployment.

## Adapter development checklist

When creating a new adapter, use this checklist:

- create `adapters/<name>/`
- add reusable logic under `include/` and `src/`
- add a shared adapter config helper if both standalone and bundled modes need the same config interpretation
- keep `main.cpp` thin
- add tests under `tests/`
- add adapter-local config example
- add standalone `Dockerfile`
- if single-shipping is required, add:
  - `bundled_entry.cpp`
  - `BundleInject.cmake`
  - `Dockerfile.bundle`
- document the adapter in its local `README.md`

## Example to follow

Use the demo adapter as the reference implementation for:

- adapter directory layout
- standalone build structure
- bundled build injection
- bundled Docker packaging
- adapter-specific Compose packaging

Relevant reference files:

- `adapters/demo-qod-adapter/CMakeLists.txt`
- `adapters/demo-qod-adapter/BundleInject.cmake`
- `adapters/demo-qod-adapter/include/adapter_config.hpp`
- `adapters/demo-qod-adapter/src/main.cpp`
- `adapters/demo-qod-adapter/src/adapter_config.cpp`
- `adapters/demo-qod-adapter/src/bundled_entry.cpp`
- `adapters/demo-qod-adapter/Dockerfile`
- `adapters/demo-qod-adapter/Dockerfile.bundle`
- `docker-compose/docker-compose-demo-qod-bundled.yaml`

## Notes

- A new adapter does not need bundled support on day one.
- Standalone mode is usually the fastest path to a working integration.
- Add bundled support only when there is a real packaging or deployment requirement for single shipping.
