#!/usr/bin/env bash
# Local pre-push verification — mirrors the staged CI pipeline
# (.github/workflows/ci.yml) closely enough to catch most failures before
# you push, without invoking GitHub Actions itself.
#
# Usage:
#   .github/scripts/pre-push.sh [--base <ref>] [--quick] [--coverage]
#                                [--component <name>] [--fix]
#
# Default (no flags): format + trailing-whitespace/permissions +
# checks (clang-tidy + unit tests) + build, for whichever components changed
# vs --base (default origin/main). Docker-based checks are skipped per
# component with no relevant changes, same as the CI matrices.
#
#   --quick               Skip the Docker-based tier entirely (format +
#                          whitespace/permissions only — seconds, no Docker).
#   --coverage             Also build the coverage stage. Off by default —
#                          it's report-only in CI and the slowest check
#                          (needs a fresh -O0 recompile, not an incremental
#                          add-on like checks).
#   --component <name>     Restrict the Docker tier to one component
#                          (phine.af-core|pcf-handler|phine.af-demo-qod-adapter|bundled-af)
#                          instead of auto-detecting from the diff. Useful
#                          when you know exactly what you touched and don't
#                          want to wait on the diff-based skip logic.
#   --fix                  Passed through to check_format.sh and
#                          check_whitespace.sh.
#
# NOT covered: Integration Tests / Tutorials Validation — 20-45 min E2E
# suites needing a full 5G network stack + gtp5g kernel module. See
# docs/contributing.md to run those manually if you need to.

set -uo pipefail # deliberately not -e: keep going and report a full summary

BASE_REF="origin/main"
QUICK=false
WITH_COVERAGE=false
ONLY_COMPONENT=""
FIX_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
    --base)
        BASE_REF="$2"
        shift 2
        ;;
    --quick)
        QUICK=true
        shift
        ;;
    --coverage)
        WITH_COVERAGE=true
        shift
        ;;
    --component)
        ONLY_COMPONENT="$2"
        shift 2
        ;;
    --fix)
        FIX_ARGS+=(--fix)
        shift
        ;;
    *)
        echo "Unknown argument: $1" >&2
        exit 1
        ;;
    esac
done

RESULTS=()
record() { RESULTS+=("$1|$2"); }

banner() {
    echo
    echo "=================================================="
    echo " $1"
    echo "=================================================="
}

banner "Format Check"
if .github/scripts/check_format.sh "${FIX_ARGS[@]}" --base "$BASE_REF"; then
    record "Format Check" "PASS"
else
    record "Format Check" "FAIL"
fi

banner "Trailing Whitespace / File Permissions"
if .github/scripts/check_whitespace.sh "${FIX_ARGS[@]}" --base "$BASE_REF"; then
    record "Whitespace/Permissions" "PASS"
else
    record "Whitespace/Permissions" "FAIL"
fi

if $QUICK; then
    echo
    echo "--quick: skipping checks and build."
else
    if ! command -v docker >/dev/null 2>&1; then
        echo "error: docker is required for checks/build (or pass --quick)" >&2
        exit 1
    fi

    # name:dockerfile:diff-path — used for checks and build.
    COMPONENTS=(
        "phine.af-core:af_core/Dockerfile:af_core"
        "pcf-handler:southbound/pcf_handler/Dockerfile:southbound/pcf_handler"
        "phine.af-demo-qod-adapter:adapters/demo-qod-adapter/Dockerfile:adapters/demo-qod-adapter"
    )
    # Tidy-only — src/ (the bundled runtime) has no unit-test suite, so its
    # `checks` stage is clang-tidy only (see checks.yml's has_tests: false).
    TIDY_ONLY_COMPONENTS=(
        "bundled-af:Dockerfile:src"
    )

    component_selected() {
        local name="$1" diff_path="$2"
        if [[ -n "$ONLY_COMPONENT" ]]; then
            [[ "$ONLY_COMPONENT" == "$name" ]]
            return
        fi
        ! git diff --quiet "$BASE_REF...HEAD" -- "$diff_path" 2>/dev/null
    }

    # Checks stage merges clang-tidy and unit tests into one image: tests
    # run at build time (a failing test fails the build), then a diff is
    # piped into the built image to run clang-tidy. has_tests=false skips
    # the unit-test result (e.g. bundled-af, which has no test suite).
    run_checks() {
        local name="$1" dockerfile="$2" diff_path="$3" has_tests="$4"
        if ! component_selected "$name" "$diff_path"; then
            record "Static Analysis ($name)" "SKIPPED (no changes)"
            [[ "$has_tests" == true ]] && record "Unit Tests ($name)" "SKIPPED (no changes)"
            return
        fi
        banner "Checks: $name"
        if docker build --target checks -f "$dockerfile" -t "$name:pre-push-checks" .; then
            [[ "$has_tests" == true ]] && record "Unit Tests ($name)" "PASS"
            if git diff --no-prefix -U0 "$BASE_REF" -- "$diff_path" | docker run --rm -i "$name:pre-push-checks"; then
                record "Static Analysis ($name)" "PASS"
            else
                record "Static Analysis ($name)" "FAIL"
            fi
        else
            [[ "$has_tests" == true ]] && record "Unit Tests ($name)" "FAIL"
            record "Static Analysis ($name)" "SKIPPED (build failed)"
        fi
    }

    run_build() {
        local name="$1" dockerfile="$2" diff_path="$3"
        if ! component_selected "$name" "$diff_path"; then
            record "Build ($name)" "SKIPPED (no changes)"
            return
        fi
        banner "Build: $name"
        if docker build -f "$dockerfile" -t "$name:pre-push" .; then
            record "Build ($name)" "PASS"
        else
            record "Build ($name)" "FAIL"
        fi
    }

    run_coverage() {
        local name="$1" dockerfile="$2" diff_path="$3"
        if ! component_selected "$name" "$diff_path"; then
            record "Coverage ($name)" "SKIPPED (no changes)"
            return
        fi
        banner "Coverage: $name"
        if docker build --target coverage -f "$dockerfile" -t "$name:pre-push-coverage" .; then
            record "Coverage ($name)" "PASS"
        else
            record "Coverage ($name)" "FAIL"
        fi
    }

    for entry in "${COMPONENTS[@]}"; do
        IFS=":" read -r name dockerfile diff_path <<<"$entry"
        run_checks "$name" "$dockerfile" "$diff_path" true
    done

    for entry in "${TIDY_ONLY_COMPONENTS[@]}"; do
        IFS=":" read -r name dockerfile diff_path <<<"$entry"
        run_checks "$name" "$dockerfile" "$diff_path" false
    done

    for entry in "${COMPONENTS[@]}"; do
        IFS=":" read -r name dockerfile diff_path <<<"$entry"
        run_build "$name" "$dockerfile" "$diff_path"
        if $WITH_COVERAGE; then
            run_coverage "$name" "$dockerfile" "$diff_path"
        fi
    done
fi

banner "Summary"
OVERALL=0
for r in "${RESULTS[@]}"; do
    IFS="|" read -r label status <<<"$r"
    printf '  %-30s %s\n' "$label" "$status"
    [[ "$status" == "FAIL" ]] && OVERALL=1
done

echo
if [[ $OVERALL -ne 0 ]]; then
    echo "Some checks failed — fix before pushing."
else
    echo "All checks passed."
fi

exit $OVERALL
