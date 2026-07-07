# Contributing

<!---
TODO: Add repository-level contribution guidelines.
The root README currently says "Contribution guidelines will be added here".
Source: [README.md](../README.md).
-->

## Notes

- This repository currently does not include a top-level `CONTRIBUTING.md` for `phine.af`.

<!---
TODO: Decide contribution workflow (branching) and add it here.
-->

## Code Formatting

Pull requests are checked for formatting by the `Code Format Check` GitHub
Actions workflow ([`.github/workflows/code-format.yml`](https://github.com/phinetech/phine.af/blob/main/.github/workflows/code-format.yml)),
which only checks files changed relative to the PR's base branch — not the
whole repository (submodules and vendored scripts under
`docker-compose/healthscripts/` are excluded entirely).

| Language | Tool | Config |
|---|---|---|
| C/C++ (`.c`, `.cc`, `.cpp`, `.h`, `.hpp`) | [clang-format](https://clang.llvm.org/docs/ClangFormat.html) 18 | [`.clang-format`](https://github.com/phinetech/phine.af/blob/main/.clang-format) |
| Shell (`.sh`, `.bash`) | [shfmt](https://github.com/mvdan/sh) 3.9.0 | [`.editorconfig`](https://github.com/phinetech/phine.af/blob/main/.editorconfig) |
| Python (`.py`) | [ruff format](https://docs.astral.sh/ruff/formatter/) 0.6.9 | [`ruff.toml`](https://github.com/phinetech/phine.af/blob/main/ruff.toml) |

### Installing the tools locally

```bash
# clang-format 18 — installed as a versioned binary, doesn't touch your
# system default `clang-format` if you have a different version installed
sudo apt-get install clang-format-18

# shfmt (requires Go, preinstalled on most dev setups)
go install mvdan.cc/sh/v3/cmd/shfmt@v3.9.0

# ruff
pip install ruff==0.6.9
```

Keep versions pinned to what CI uses (see the table above / the workflow
file) so a local pass guarantees a CI pass.

### Running the check

```bash
# Check formatting of files changed relative to main (no changes made)
.github/scripts/check_format.sh --base origin/main

# Auto-fix formatting in place
.github/scripts/check_format.sh --fix --base origin/main
```

Use `--base <ref>` to point at whatever branch your changes will actually
be merged into (e.g. `--base origin/feat-http-injection` if you're stacked
on top of a feature branch rather than `main`).

The script auto-detects a `clang-format-18` binary on `PATH` in preference
to a plain `clang-format`; override with `CLANG_FORMAT_BIN=...` if your
setup differs.

## Static Analysis

Pull requests are checked by the `Static Analysis (clang-tidy)` GitHub
Actions workflow ([`.github/workflows/static-analysis.yml`](https://github.com/phinetech/phine.af/blob/main/.github/workflows/static-analysis.yml)),
using [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) 18 with the
checks configured in [`.clang-tidy`](https://github.com/phinetech/phine.af/blob/main/.clang-tidy)
(`bugprone-*`, `performance-*`, `clang-analyzer-core.*` for now — see the
`TODO` in that file for how the check set is expected to grow over time).
Deliberately scoped to the analyzer's `core` package rather than the full
`clang-analyzer-*` wildcard — the full set runs expensive path-sensitive
analysis across every reachable header and is unusably slow/noisy on this
project's gRPC/protobuf/OAI/Boost dependency graph.

Like the format check, this only looks at **changed lines**, not whole
files — a finding on a line you didn't touch never fails your PR, even in a
file you otherwise edited. Only new findings in code you actually wrote are
enforced.

Covered components: `af_core`, `southbound/pcf_handler`,
`adapters/demo-qod-adapter`, and the bundled AF runtime (`src/`).
`northbound/api_component` is excluded — its Docker build isn't wired into
CI yet (see the commented-out matrix entry in
[`build.yml`](https://github.com/phinetech/phine.af/blob/main/.github/workflows/build.yml)).

### Why this doesn't run like the format check

Unlike `clang-format`, `clang-tidy` needs a real compile database
(`compile_commands.json`) with correct include paths and defines — and this
project's headers (gRPC, OAI models, nghttp2, ...) only exist inside the
project's Docker build images. There's no lightweight "install a couple of
apt packages and run" path here; each check has to build the relevant
component's Docker image first.

### Running it locally

Pick the Dockerfile for the component you changed, build its
`static-analysis` target (this runs a full build, so it can take a while
the first time), then pipe a diff into the resulting image:

```bash
# af_core
docker build --target static-analysis -f af_core/Dockerfile -t af-core:tidy .
git diff --no-prefix -U0 origin/main -- af_core | docker run --rm -i af-core:tidy

# southbound/pcf_handler
docker build --target static-analysis -f southbound/pcf_handler/Dockerfile -t pcf-handler:tidy .
git diff --no-prefix -U0 origin/main -- southbound/pcf_handler | docker run --rm -i pcf-handler:tidy

# adapters/demo-qod-adapter
docker build --target static-analysis -f adapters/demo-qod-adapter/Dockerfile -t demo-qod-adapter:tidy .
git diff --no-prefix -U0 origin/main -- adapters/demo-qod-adapter | docker run --rm -i demo-qod-adapter:tidy

# bundled AF runtime (src/)
docker build --target static-analysis -f Dockerfile -t bundled-af:tidy .
git diff --no-prefix -U0 origin/main -- src | docker run --rm -i bundled-af:tidy
```

Swap `origin/main` for whatever branch your changes will actually be merged
into (e.g. `origin/feat-http-injection` if you're stacked on a feature
branch). If the diff for a given path is empty, the container has nothing
to check and exits cleanly.

No output means no new findings. Findings are printed with file/line
locations, same as running `clang-tidy` directly.

## Testing

Pull requests run a fast unit-test job (`Unit Tests` — [`.github/workflows/unit-tests.yml`](https://github.com/phinetech/phine.af/blob/main/.github/workflows/unit-tests.yml))
across the components that have unit suites. Tests are written with
[GoogleTest](https://google.github.io/googletest/) and run through
[CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html) —
`BUILD_TESTING` (the standard CTest convention) gates whether each
component's `tests/` subdirectory is even configured, and every suite is
tagged with a CTest **label** so CI can select "fast" vs. "heavy":

| Component | Suite | Label | Runs in CI via |
|---|---|---|---|
| `af_core` | `tests/unit` — light scaffold, one real class covered so far; more coverage to come | `unit` | `unit-tests.yml`, every PR |
| `af_core` | `tests/integration` — full 5G network (free5gc, gNB, UE) | `integration` | `integration-tests.yml` (unchanged, heavy, separate) |
| `southbound/pcf_handler` | `tests/` — flow-description utilities | `unit` | `unit-tests.yml`, every PR |
| `adapters/demo-qod-adapter` | `tests/` — QoD client + session manager (gmock) | `unit` | `unit-tests.yml`, every PR |

`northbound/api_component` has no unit suite and is excluded, matching its
exclusion from `build.yml`'s component matrix.

### Why Docker again

Same reason as static analysis: each component's unit tests link against
that component's real library (`af_core_lib`, etc.), which pulls in gRPC,
OAI models, or other dependencies that only exist inside that component's
Docker build. Each Dockerfile has a `tests` stage (`FROM builder AS tests`)
that installs `libgtest-dev`/`libgmock-dev` from apt (not `FetchContent` —
avoids a build-time network clone and the `build/_deps` committed-gitlink
problem that broke checkout once already), reconfigures with
`-DBUILD_TESTING=ON`, and runs `ctest -L unit --output-on-failure` as part
of the image build itself — a failing test fails the `docker build`.

### Running it locally

```bash
# af_core
docker build --target tests -f af_core/Dockerfile -t af-core:tests .

# southbound/pcf_handler
docker build --target tests -f southbound/pcf_handler/Dockerfile -t pcf-handler:tests .

# adapters/demo-qod-adapter
docker build --target tests -f adapters/demo-qod-adapter/Dockerfile -t demo-qod-adapter:tests .
```

Unlike the format/clang-tidy checks, there's no separate `docker run` step —
the tests execute during the build, and the build fails if any test does.

If you already have a component's full native dependency stack installed
(the same libraries its Dockerfile installs — gRPC, protobuf, OAI models,
etc.), you can skip Docker entirely:

```bash
cmake -S southbound/pcf_handler -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build -L unit --output-on-failure
```

### Adding a new test

Add a `TEST()`/`TEST_F()` case to the relevant component's `tests/`
directory (or a new source file, added to that directory's `CMakeLists.txt`)
and tag it via the suite's `gtest_discover_tests(... PROPERTIES LABELS
"unit")` call — no CI changes needed, `unit-tests.yml` picks up anything
under `-L unit` automatically.

## Coverage

Pull requests get a `gcov`-based coverage report (via
[`gcovr`](https://gcovr.com/)) for the same 3 components as the unit-test
job — [`.github/workflows/coverage.yml`](https://github.com/phinetech/phine.af/blob/main/.github/workflows/coverage.yml)
uploads an HTML report per component as a build artifact. This is
**report-only** — coverage never fails a PR. Given today's suites are
light/narrow (see the Testing section above), a hard threshold would block
unrelated PRs; this can be revisited once coverage is more established.
Each Dockerfile's `coverage` stage has a `TODO` with the commented-out
`gcovr --fail-under-*` flags ready to re-enable gating later.

Coverage requires `-O0 --coverage`, which is incompatible with the
`-O3` used everywhere else — so unlike the `tests` stage, `coverage` isn't
a quick incremental add-on. It's a genuine (if bounded — a couple of
minutes, not a from-scratch rebuild) recompile of the component's own
library with different flags, on its own `FROM builder AS coverage`
Docker stage.

### Running it locally

```bash
# af_core
docker build --target coverage -f af_core/Dockerfile -t af-core:coverage .

# southbound/pcf_handler
docker build --target coverage -f southbound/pcf_handler/Dockerfile -t pcf-handler:coverage .

# adapters/demo-qod-adapter
docker build --target coverage -f adapters/demo-qod-adapter/Dockerfile -t demo-qod-adapter:coverage .
```

Like `tests`, coverage runs during the build itself (`gcovr --print-summary`
prints the headline numbers to the build log). To view the HTML report,
extract it after building:

```bash
docker create --name extract-cov af-core:coverage
docker cp extract-cov:/app/build-output/coverage.html ./coverage.html
docker rm extract-cov
```

(swap the container path for `/app/southbound/pcf_handler/build-output/coverage.html`
or `/app/adapters/demo-qod-adapter/build/coverage.html` for the other two).

### A `gcovr` gotcha worth knowing if you touch the filters

`gcovr`'s `--exclude`/`--filter` patterns behave differently depending on
whether they start with `/`: a pattern *without* a leading `/` gets
resolved relative to gcovr's **current working directory** (not `--root`,
and not the file being tested) — if the candidate file isn't under that
directory, the pattern silently never matches it, no error. Source files
that live outside the build directory (e.g. `tests/`, which sits next to
`build/`, not inside it) need a **fully-qualified absolute pattern**
(e.g. `/app/adapters/demo-qod-adapter/tests/.*`) to match reliably — this
is why the Dockerfiles' `gcovr` invocations mix styles rather than using
one consistent regex shape.
