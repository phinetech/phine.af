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

## CI Pipeline

[`.github/workflows/ci.yml`](https://github.com/phinetech/phine.af/blob/main/.github/workflows/ci.yml)
is the single entry point for pull requests — it orchestrates every check
below as a staged pipeline rather than running everything in parallel, so
a fast lint failure means the 20-45 minute E2E suites never even start:

1. **Format Check** — fast lint, no Docker. Runs on every push including
   draft PRs.
2. **Build**, **Checks** (clang-tidy + unit tests) — run in parallel,
   gated on Format Check. Each uses the shared GHA layer cache so
   `builder` layers built by one job are reused by the other.
   **Checks runs on draft PRs; Build does not.**
3. **Coverage** — gated on Checks. Skipped on draft PRs.
4. **Integration Tests**, **Tutorials Validation** — the heaviest, full
   E2E checks. Gated on everything above. Skipped on draft PRs.
5. **Publish** — pushes images to `phinetech/` on merges to `main`/`develop`
   only (never on PRs).

In short: **draft PRs run format check + unit tests + clang-tidy only**.
Mark a PR ready for review to trigger the full pipeline (GitHub Actions
re-triggers the workflow on the `ready_for_review` event — no new commit
need).

Each tier is defined in its own workflow file for readability, but
they're all [reusable workflows](https://docs.github.com/en/actions/using-workflows/reusing-workflows)
(`on: workflow_call`) rather than being triggered directly — `ci.yml` is
what actually invokes them, via `needs:` to enforce the ordering. If a
`needs:` job fails, GitHub Actions skips (not fails-then-continues) every
job depending on it — that's the mechanism this relies on, not custom
scripting. If you're debugging why a workflow file didn't run on your PR,
check `ci.yml` rather than the file itself.

### Running it locally before you push

[`.github/scripts/pre-push.sh`](https://github.com/phinetech/phine.af/blob/main/.github/scripts/pre-push.sh)
covers the same ground as tiers 1-2 of the pipeline above — format,
trailing whitespace/permissions, static analysis, unit tests, and a build
— without needing to invoke GitHub Actions itself:

```bash
# Default: format + whitespace/permissions + checks (clang-tidy + unit tests)
# + build, for whichever components changed vs origin/main
.github/scripts/pre-push.sh

# Fast-only: format + whitespace/permissions, skip the Docker-based tier
.github/scripts/pre-push.sh --quick

# Also build the coverage stage (off by default — report-only in CI, and
# the slowest check since it needs a fresh -O0 recompile)
.github/scripts/pre-push.sh --coverage

# Restrict the Docker tier to one component instead of auto-detecting
# from the diff — useful when you know exactly what you touched
.github/scripts/pre-push.sh --component af-core

# Diff against a different base (e.g. stacked on a feature branch)
.github/scripts/pre-push.sh --base origin/feat-http-injection

# Auto-fix formatting and whitespace issues along the way
.github/scripts/pre-push.sh --fix
```

Docker-based checks are skipped per-component when that component has no
relevant changes, same skip logic the CI matrices use — so this doesn't
cost you 3 components' worth of build time when you only touched one.
Ends with a summary of every check that ran, skipped, passed, or failed,
and exits non-zero if anything failed.

`--fix` (and `check_format.sh`/`check_whitespace.sh`'s own `--fix` flags)
only ever touch lines that are actually part of your diff against
`--base` — for whitespace specifically, that means stripping trailing
whitespace exactly on the lines `git diff --check` flagged (or trimming
one-or-more trailing blank lines at EOF down to exactly one newline),
never a whole-file reformat that could touch pre-existing, unrelated
lines. One nuance: the checks compare **committed** refs
(`base...HEAD`), not your working tree, so after `--fix` changes files
on disk you need to `git add`/commit (or amend) before re-running the
check to see it reflect as clean — this is the same "why doesn't my fix
show up" gotcha as any `base...HEAD`-style diff, not something specific
to this script.

**Not covered**: Integration Tests and Tutorials Validation (tier 4) —
20-45 minute E2E suites needing a full 5G network stack and the `gtp5g`
kernel module. Not realistic to run before every push — see
[`af_core/tests/integration/README.md`](https://github.com/phinetech/phine.af/blob/main/af_core/tests/integration/README.md)
if you need to run those manually.

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

Pull requests are checked by the `Checks` GitHub Actions workflow
([`.github/workflows/checks.yml`](https://github.com/phinetech/phine.af/blob/main/.github/workflows/checks.yml)),
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
`checks` target (this runs a full build plus the unit tests, so it can
take a while the first time), then pipe a diff into the resulting image:

```bash
# af_core
docker build --target checks -f af_core/Dockerfile -t af-core:checks .
git diff --no-prefix -U0 origin/main -- af_core | docker run --rm -i af-core:checks

# southbound/pcf_handler
docker build --target checks -f southbound/pcf_handler/Dockerfile -t pcf-handler:checks .
git diff --no-prefix -U0 origin/main -- southbound/pcf_handler | docker run --rm -i pcf-handler:checks

# adapters/demo-qod-adapter
docker build --target checks -f adapters/demo-qod-adapter/Dockerfile -t demo-qod-adapter:checks .
git diff --no-prefix -U0 origin/main -- adapters/demo-qod-adapter | docker run --rm -i demo-qod-adapter:checks

# bundled AF runtime (src/) — tidy only, no unit suite
docker build --target checks -f Dockerfile -t bundled-af:checks .
git diff --no-prefix -U0 origin/main -- src | docker run --rm -i bundled-af:checks
```

Swap `origin/main` for whatever branch your changes will actually be merged
into (e.g. `origin/feat-http-injection` if you're stacked on a feature
branch). If the diff for a given path is empty, the container has nothing
to check and exits cleanly.

No output means no new findings. Findings are printed with file/line
locations, same as running `clang-tidy` directly.

## Testing

Pull requests run a fast checks job (`Checks` — [`.github/workflows/checks.yml`](https://github.com/phinetech/phine.af/blob/main/.github/workflows/checks.yml))
across the components that have unit suites. Tests are written with
[GoogleTest](https://google.github.io/googletest/) and run through
[CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html) —
`BUILD_TESTING` (the standard CTest convention) gates whether each
component's `tests/` subdirectory is even configured, and every suite is
tagged with a CTest **label** so CI can select "fast" vs. "heavy":

| Component | Suite | Label | Runs in CI via |
|---|---|---|---|
| `af_core` | `tests/unit` — light scaffold, one real class covered so far; more coverage to come | `unit` | `checks.yml`, every PR (including drafts) |
| `af_core` | `tests/integration` — full 5G network (free5gc, gNB, UE) | `integration` | `integration-tests.yml` (unchanged, heavy, separate) |
| `southbound/pcf_handler` | `tests/` — flow-description utilities | `unit` | `checks.yml`, every PR (including drafts) |
| `adapters/demo-qod-adapter` | `tests/` — QoD client + session manager (gmock) | `unit` | `checks.yml`, every PR (including drafts) |

`northbound/api_component` has no unit suite and is excluded, matching its
exclusion from `build.yml`'s component matrix.

### Why Docker again

Same reason as static analysis: each component's unit tests link against
that component's real library (`af_core_lib`, etc.), which pulls in gRPC,
OAI models, or other dependencies that only exist inside that component's
Docker build. Each Dockerfile has a `checks` stage (`FROM builder AS checks`)
that installs `libgtest-dev`/`libgmock-dev` and `clang-tidy-18` from apt
in a single layer, reconfigures with `-DBUILD_TESTING=ON`, runs
`ctest -L unit --output-on-failure` as part of the image build — a failing
test fails the `docker build` — and sets `ENTRYPOINT check_tidy.sh` so
the same image handles the clang-tidy pass too.

### Running it locally

```bash
# af_core
docker build --target checks -f af_core/Dockerfile -t af-core:checks .

# southbound/pcf_handler
docker build --target checks -f southbound/pcf_handler/Dockerfile -t pcf-handler:checks .

# adapters/demo-qod-adapter
docker build --target checks -f adapters/demo-qod-adapter/Dockerfile -t demo-qod-adapter:checks .
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
**report-only** — coverage never fails a PR. Coverage is skipped on draft
PRs. Given today's suites are light/narrow (see the Testing section
above), a hard threshold would block unrelated PRs; this can be revisited
once coverage is more established. Each Dockerfile's `coverage` stage has
a `TODO` with the commented-out `gcovr --fail-under-*` flags ready to
re-enable gating later.

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
