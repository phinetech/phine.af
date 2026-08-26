# Contributing

## CI Pipeline

[`.github/workflows/ci.yml`](https://github.com/phinetech/phine.af/blob/main/.github/workflows/ci.yml)
is the single entry point for pull requests — a staged pipeline where a fast
failure prevents later, heavier stages from running:

1. **Format Check** — fast lint, no Docker. Runs on every push including draft PRs.
2. **Build**, **Checks** (clang-tidy + unit tests) — run in parallel, gated on Format Check. **Checks runs on draft PRs; Build does not.**
3. **Coverage** — gated on Checks. Skipped on draft PRs.
4. **Integration Tests**, **Tutorials Validation** — full E2E checks. Gated on everything above. Skipped on draft PRs.
5. **Publish** — merges to `main`/`develop` only.

**Draft PRs run format check + unit tests + clang-tidy only.** Mark a PR ready for review to trigger the full pipeline.

### Running locally before you push

[`.github/scripts/pre-push.sh`](https://github.com/phinetech/phine.af/blob/main/.github/scripts/pre-push.sh)
covers the same ground as tiers 1–2:

```bash
# Default: format + whitespace/permissions + checks + build
.github/scripts/pre-push.sh

# Format + whitespace only, skip Docker-based checks
.github/scripts/pre-push.sh --quick

# Include coverage stage
.github/scripts/pre-push.sh --coverage

# Restrict to one component
.github/scripts/pre-push.sh --component af-core

# Diff against a different base
.github/scripts/pre-push.sh --base origin/feat-http-injection

# Auto-fix formatting and whitespace
.github/scripts/pre-push.sh --fix
```

> **Note:** `--fix` compares committed refs (`base...HEAD`), not your working tree — after it changes files you need to `git add`/commit before re-running the check.

Integration Tests and Tutorials Validation (tier 4) are not covered — see
[`af_core/tests/integration/README.md`](https://github.com/phinetech/phine.af/blob/main/af_core/tests/integration/README.md)
to run those manually.

## Code Formatting

Only files changed relative to the PR's base branch are checked.

| Language | Tool | Config |
|---|---|---|
| C/C++ (`.c`, `.cc`, `.cpp`, `.h`, `.hpp`) | [clang-format](https://clang.llvm.org/docs/ClangFormat.html) 18 | [`.clang-format`](https://github.com/phinetech/phine.af/blob/main/.clang-format) |
| Shell (`.sh`, `.bash`) | [shfmt](https://github.com/mvdan/sh) 3.9.0 | [`.editorconfig`](https://github.com/phinetech/phine.af/blob/main/.editorconfig) |
| Python (`.py`) | [ruff format](https://docs.astral.sh/ruff/formatter/) 0.6.9 | [`ruff.toml`](https://github.com/phinetech/phine.af/blob/main/ruff.toml) |

```bash
# Install tools (pin to versions above to match CI)
sudo apt-get install clang-format-18
go install mvdan.cc/sh/v3/cmd/shfmt@v3.9.0
pip install ruff==0.6.9
```

```bash
# Check formatting
.github/scripts/check_format.sh --base origin/main

# Auto-fix in place
.github/scripts/check_format.sh --fix --base origin/main
```

## Static Analysis

[clang-tidy](https://clang.llvm.org/extra/clang-tidy/) 18 checks changed lines only — findings on untouched lines never fail a PR.

Covered components: `af_core`, `southbound/pcf_handler`, `adapters/demo-qod-adapter`, `src/` (bundled AF runtime).

clang-tidy requires the component's Docker build image (for correct include paths). Build the `checks` target and pipe a diff into it:

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

# bundled AF runtime (src/)
docker build --target checks -f Dockerfile -t bundled-af:checks .
git diff --no-prefix -U0 origin/main -- src | docker run --rm -i bundled-af:checks
```

No output means no new findings.

## Testing

Tests use [GoogleTest](https://google.github.io/googletest/) / [CTest](https://cmake.org/cmake/help/latest/manual/ctest.1.html), tagged with CTest labels:

| Component | Suite | Label |
|---|---|---|
| `af_core` | `tests/unit` | `unit` |
| `af_core` | `tests/integration` — full 5G network | `integration` |
| `southbound/pcf_handler` | `tests/` | `unit` |
| `adapters/demo-qod-adapter` | `tests/` | `unit` |

Unit tests run as part of the `checks` Docker build — a failing test fails the build:

```bash
docker build --target checks -f af_core/Dockerfile -t af-core:checks .
docker build --target checks -f southbound/pcf_handler/Dockerfile -t pcf-handler:checks .
docker build --target checks -f adapters/demo-qod-adapter/Dockerfile -t demo-qod-adapter:checks .
```

If you have native dependencies installed, you can skip Docker:

```bash
cmake -S southbound/pcf_handler -B build -DBUILD_TESTING=ON
cmake --build build -j
ctest --test-dir build -L unit --output-on-failure
```

### Adding a new test

Add a `TEST()`/`TEST_F()` case to the component's `tests/` directory and tag it via `gtest_discover_tests(... PROPERTIES LABELS "unit")` — no CI changes needed.

## Coverage

Coverage reports (`gcov`/[`gcovr`](https://gcovr.com/)) are uploaded as build artifacts — **report-only, never fails a PR**. Skipped on draft PRs.

```bash
docker build --target coverage -f af_core/Dockerfile -t af-core:coverage .
docker build --target coverage -f southbound/pcf_handler/Dockerfile -t pcf-handler:coverage .
docker build --target coverage -f adapters/demo-qod-adapter/Dockerfile -t demo-qod-adapter:coverage .
```

To view the HTML report after building:

```bash
docker create --name extract-cov af-core:coverage
docker cp extract-cov:/app/build-output/coverage.html ./coverage.html
docker rm extract-cov
```

(Swap the container path for `/app/southbound/pcf_handler/build-output/coverage.html` or `/app/adapters/demo-qod-adapter/build/coverage.html` for the other components.)
