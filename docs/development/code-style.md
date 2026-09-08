# Code style

## C++ standard

This repo targets **C++17** (see the root [`CMakeLists.txt`](../../CMakeLists.txt)).

## Formatting

Formatting is enforced in CI. Configuration lives at the repo root:

- C/C++: [`.clang-format`](../../.clang-format) — clang-format 18
- Shell: [`.editorconfig`](../../.editorconfig) — shfmt 3.9.0
- Python: [`ruff.toml`](../../ruff.toml) — ruff 0.6.9

Static analysis: [`.clang-tidy`](../../.clang-tidy) — clang-tidy 18.

Only files changed relative to the PR's base branch are checked. See
[Contributing](../contributing.md) for how to run the format/tidy checks
locally.