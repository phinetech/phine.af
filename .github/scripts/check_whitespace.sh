#!/usr/bin/env bash
# Checks (or fixes) trailing whitespace and shell-script executable
# permissions on files changed relative to a base ref. Shared between
# build.yml (CI) and pre-push.sh (local) so this logic has one source of
# truth.
#
# Usage:
#   .github/scripts/check_whitespace.sh [--fix] --base <ref>
#
# --fix strips exactly the lines git diff --check flagged (trailing
# whitespace on a line, or one-or-more trailing blank lines at EOF) —
# not the whole file — so it only ever touches lines that are part of
# the diff against --base, same scoping as the check itself. Anything
# git diff --check flags that isn't one of those two known categories is
# left alone and reported, not silently dropped. Also chmod +x's any
# changed shell script missing the executable bit.
#
# The caller is responsible for resolving --base to something diffable
# (e.g. build.yml falls back to HEAD^ for push events, where there's no
# PR base ref) — this script doesn't guess.

set -euo pipefail

FIX=false
BASE_REF=""

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

if [[ -z "$BASE_REF" ]]; then
    echo "error: --base <ref> is required" >&2
    exit 1
fi

STATUS=0

echo "== Trailing whitespace (vs $BASE_REF) =="
set +e
OUTPUT="$(git diff --check "$BASE_REF...HEAD")"
DIFF_CHECK_STATUS=$?
set -e

if [[ $DIFF_CHECK_STATUS -eq 0 ]]; then
    echo "No trailing whitespace found"
elif $FIX; then
    echo "$OUTPUT"
    echo
    echo "Fixing flagged issues..."

    # `|| true` on each block below: grep exits 1 when a diff has none of
    # that category's findings (e.g. only blank-line-at-EOF issues, zero
    # trailing-whitespace ones), and set -e would otherwise kill the
    # script right there, silently skipping every fix after it.
    echo "$OUTPUT" | grep -E '^[^:]+:[0-9]+: trailing whitespace\.$' | while IFS=: read -r file line _; do
        sed -i "${line}s/[ \t]*\$//" "$file"
        echo "  stripped trailing whitespace: $file:$line"
    done || true

    # One or more trailing blank lines at EOF — normalize to exactly one
    # trailing newline regardless of how many blank lines were reported.
    echo "$OUTPUT" | grep -E '^[^:]+:[0-9]+: new blank line at EOF\.$' | cut -d: -f1 | sort -u | while read -r file; do
        printf '%s\n' "$(cat "$file")" >"$file"
        echo "  removed trailing blank line(s): $file"
    done || true

    UNHANDLED="$(echo "$OUTPUT" | grep -vE '^[^:]+:[0-9]+: (trailing whitespace|new blank line at EOF)\.$' | grep -vE '^\+' || true)"
    if [[ -n "$UNHANDLED" ]]; then
        echo
        echo "warning: some findings aren't auto-fixable, please fix manually:"
        echo "$UNHANDLED"
        STATUS=1
    fi

    echo
    echo "Done. Re-run without --fix to verify."
else
    echo "$OUTPUT"
    echo
    echo "Files with trailing whitespace:"
    echo "$OUTPUT" | grep -vE '^\+' | grep -oE '^[^:]+' | sort -u | sed 's/^/  - /'
    STATUS=1
fi

echo
echo "== Shell script executable permissions (vs $BASE_REF) =="
mapfile -t CHANGED_SH_FILES < <(git diff --name-only --diff-filter=ACMR "$BASE_REF...HEAD" -- '*.sh' || true)

if [[ ${#CHANGED_SH_FILES[@]} -eq 0 ]]; then
    echo "No shell scripts changed"
else
    NON_EXEC=()
    for f in "${CHANGED_SH_FILES[@]}"; do
        [[ -f "$f" ]] || continue
        [[ -x "$f" ]] || NON_EXEC+=("$f")
    done

    if [[ ${#NON_EXEC[@]} -eq 0 ]]; then
        echo "All changed shell scripts are executable"
    elif $FIX; then
        echo "Setting the executable bit..."
        for f in "${NON_EXEC[@]}"; do
            chmod +x "$f"
            echo "  fixed $f"
        done
    else
        echo "warning: shell scripts missing the executable bit (not run-blocking):"
        printf '  - %s\n' "${NON_EXEC[@]}"
    fi
fi

exit $STATUS
