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
