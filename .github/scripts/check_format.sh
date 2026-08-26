#!/usr/bin/env bash
# Formats or checks formatting for files changed relative to a base ref.
#
# Usage:
#   .github/scripts/check_format.sh [--fix] [--base <ref>]
#
# CI runs this in check-only mode against the PR's base branch. Locally,
# run with --fix to format your changes before pushing:
#   .github/scripts/check_format.sh --fix --base origin/main
#
# Dispatches by extension:
#   .c/.cc/.cpp/.h/.hpp -> clang-format (uses .clang-format)
#   .sh/.bash           -> shfmt (uses .editorconfig)
#   .py                 -> ruff format (uses ruff.toml)
#
# clang-format is pinned to a specific major version (see .clang-format
# for the version it's tuned against). Rather than requiring that version
# to be your default `clang-format`, this picks a versioned binary
# (e.g. clang-format-18) off PATH if present, so you can install it
# side by side with whatever `clang-format` already points to. Override
# with CLANG_FORMAT_BIN=... if your setup differs.

set -euo pipefail

CLANG_FORMAT_VERSION=18

if [[ -n "${CLANG_FORMAT_BIN:-}" ]]; then
    : # explicit override wins
elif command -v "clang-format-${CLANG_FORMAT_VERSION}" >/dev/null 2>&1; then
    CLANG_FORMAT_BIN="clang-format-${CLANG_FORMAT_VERSION}"
elif command -v clang-format >/dev/null 2>&1; then
    CLANG_FORMAT_BIN="clang-format"
    installed_version=$(clang-format --version | grep -oE '[0-9]+' | head -1)
    if [[ "$installed_version" != "$CLANG_FORMAT_VERSION" ]]; then
        echo "warning: using clang-format version ${installed_version}, expected ${CLANG_FORMAT_VERSION}." \
            "Install clang-format-${CLANG_FORMAT_VERSION} to match CI exactly." >&2
    fi
else
    echo "error: no clang-format found on PATH." >&2
    exit 1
fi

FIX=false
BASE_REF="origin/main"

while [[ $# -gt 0 ]]; do
    case "$1" in
    --fix)
        FIX=true
        shift
        ;;
    --base)
        BASE_REF="$2"
        shift 2
        ;;
    *)
        echo "Unknown argument: $1" >&2
        exit 1
        ;;
    esac
done

# Vendored/external paths this project doesn't own the style of.
EXCLUDE_REGEX='^(common/|southbound/oai-cn5g-common-src/|af_core/tests/integration/common/|docker-compose/healthscripts/)'

mapfile -t CHANGED_FILES < <(git diff --name-only --diff-filter=ACMR "${BASE_REF}...HEAD" -- . | grep -vE "$EXCLUDE_REGEX" || true)

if [[ ${#CHANGED_FILES[@]} -eq 0 ]]; then
    echo "No changed files to check."
    exit 0
fi

CPP_FILES=()
SHELL_FILES=()
PYTHON_FILES=()

for f in "${CHANGED_FILES[@]}"; do
    [[ -f "$f" ]] || continue
    case "$f" in
    *.c | *.cc | *.cpp | *.h | *.hpp) CPP_FILES+=("$f") ;;
    *.sh | *.bash) SHELL_FILES+=("$f") ;;
    *.py) PYTHON_FILES+=("$f") ;;
    esac
done

STATUS=0

if [[ ${#CPP_FILES[@]} -gt 0 ]]; then
    echo "== $CLANG_FORMAT_BIN (${#CPP_FILES[@]} file(s)) =="
    if $FIX; then
        "$CLANG_FORMAT_BIN" -i "${CPP_FILES[@]}"
    else
        "$CLANG_FORMAT_BIN" --dry-run --Werror "${CPP_FILES[@]}" || STATUS=1
    fi
fi

if [[ ${#SHELL_FILES[@]} -gt 0 ]]; then
    echo "== shfmt (${#SHELL_FILES[@]} file(s)) =="
    if $FIX; then
        shfmt -w "${SHELL_FILES[@]}"
    else
        shfmt -d "${SHELL_FILES[@]}" || STATUS=1
    fi
fi

if [[ ${#PYTHON_FILES[@]} -gt 0 ]]; then
    echo "== ruff format (${#PYTHON_FILES[@]} file(s)) =="
    if $FIX; then
        ruff format "${PYTHON_FILES[@]}"
    else
        ruff format --check "${PYTHON_FILES[@]}" || STATUS=1
    fi
fi

exit $STATUS
