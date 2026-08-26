#!/usr/bin/env bash
# Runs clang-tidy against changed lines only, using an existing
# compile_commands.json. Must run inside (or against) the same build
# environment that produced the compile database — this project's headers
# and third-party deps (gRPC, OAI models, nghttp2, ...) only exist there.
#
# Usage (diff piped in via stdin, generated with --no-prefix -U0 so hunk
# line numbers map directly to clang-tidy's --line-filter):
#   git diff --no-prefix -U0 "origin/${BASE}...HEAD" -- af_core \
#     | .github/scripts/check_tidy.sh --build-dir build-output
#
# Wraps LLVM's clang-tidy-diff.py, which turns unified-diff hunks into
# clang-tidy's --line-filter so only changed lines are checked — pre-existing
# findings elsewhere in a touched file are not reported. Existing code stays
# exempt; only new findings in touched lines fail the check
# (WarningsAsErrors in .clang-tidy).

set -euo pipefail

BUILD_DIR=""

while [[ $# -gt 0 ]]; do
    case "$1" in
    --build-dir)
        BUILD_DIR="$2"
        shift 2
        ;;
    *)
        echo "Unknown argument: $1" >&2
        exit 1
        ;;
    esac
done

if [[ -z "$BUILD_DIR" ]]; then
    echo "error: --build-dir <path to compile_commands.json dir> is required" >&2
    exit 1
fi

CLANG_TIDY_VERSION=18

if [[ -n "${CLANG_TIDY_BIN:-}" ]]; then
    : # explicit override wins
elif command -v "clang-tidy-${CLANG_TIDY_VERSION}" >/dev/null 2>&1; then
    CLANG_TIDY_BIN="clang-tidy-${CLANG_TIDY_VERSION}"
elif command -v clang-tidy >/dev/null 2>&1; then
    CLANG_TIDY_BIN="clang-tidy"
    installed_version=$(clang-tidy --version | grep -oE '[0-9]+' | head -1)
    if [[ "$installed_version" != "$CLANG_TIDY_VERSION" ]]; then
        echo "warning: using clang-tidy version ${installed_version}, expected ${CLANG_TIDY_VERSION}." \
            "Install clang-tidy-${CLANG_TIDY_VERSION} to match CI exactly." >&2
    fi
else
    echo "error: no clang-tidy found on PATH." >&2
    exit 1
fi

CLANG_TIDY_DIFF=""
for candidate in \
    "/usr/lib/llvm-${CLANG_TIDY_VERSION}/share/clang/clang-tidy-diff.py" \
    "$(command -v "clang-tidy-diff-${CLANG_TIDY_VERSION}.py" 2>/dev/null || true)" \
    "$(command -v clang-tidy-diff.py 2>/dev/null || true)"; do
    if [[ -n "$candidate" && -f "$candidate" ]]; then
        CLANG_TIDY_DIFF="$candidate"
        break
    fi
done

if [[ -z "$CLANG_TIDY_DIFF" ]]; then
    echo "error: clang-tidy-diff.py not found (expected alongside clang-tidy-${CLANG_TIDY_VERSION})." >&2
    exit 1
fi

DIFF_INPUT="$(cat)"

if [[ -z "$DIFF_INPUT" ]]; then
    echo "No changes to check."
    exit 0
fi

# Don't let set -e kill the script if clang-tidy-diff.py exits non-zero
# (e.g. a WarningsAsErrors match) — we need to print $OUTPUT regardless.
set +e
OUTPUT="$(echo "$DIFF_INPUT" | python3 "$CLANG_TIDY_DIFF" \
    -p0 \
    -path "$BUILD_DIR" \
    -clang-tidy-binary "$CLANG_TIDY_BIN" \
    -quiet \
    -j "$(nproc)" \
    -extra-arg=-Wno-everything 2>&1)"
TIDY_STATUS=$?
set -e

# "N warnings generated." lines are Clang's internal diagnostic tally,
# accumulated across every header pulled into the translation unit (not
# just this project's code) — .clang-tidy's HeaderFilterRegex and the
# line-filter above already keep them from surfacing as real findings, so
# they're just noise here. Strip them (and the blank lines they leave
# behind) from what gets printed; the pass/fail check below still runs
# against the unfiltered $OUTPUT.
echo "$OUTPUT" | grep -v -E '^[0-9]+ warnings? generated\.?$' | cat -s

if [[ $TIDY_STATUS -ne 0 ]] || echo "$OUTPUT" | grep -q 'error:'; then
    exit 1
fi
