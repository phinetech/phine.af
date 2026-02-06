#!/bin/bash
# Script to check for potential deadlocks using multiple methods

# Note: Not using 'set -e' so script continues even if TSan build fails

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build-output"

echo "==================================="
echo "Deadlock Detection for AF Core"
echo "==================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Method 1: Static Analysis with grep (simple pattern matching)
echo ""
echo "${YELLOW}[1/4] Static Analysis: Checking for nested lock patterns...${NC}"
echo ""

nested_locks=$(grep -r "std::lock_guard\|std::unique_lock" "${PROJECT_ROOT}/af_core" | \
    grep -A 20 "std::lock_guard\|std::unique_lock" | \
    grep -c "std::lock_guard\|std::unique_lock" || true)

if [ "$nested_locks" -gt 0 ]; then
    echo "${YELLOW}Warning: Found $nested_locks potential nested lock patterns${NC}"
    echo "Files with multiple locks:"
    grep -r -l "std::lock_guard.*std::lock_guard\|std::unique_lock.*std::lock_guard" \
        "${PROJECT_ROOT}/af_core" 2>/dev/null || echo "  (Use runtime analysis for detailed check)"
else
    echo "${GREEN}✓ No obvious nested lock patterns found${NC}"
fi

# Method 2: Check for std::scoped_lock usage (good practice)
echo ""
echo "${YELLOW}[2/4] Checking for std::scoped_lock usage (C++17)...${NC}"
echo ""

scoped_lock_count=$(grep -r "std::scoped_lock" "${PROJECT_ROOT}/af_core" 2>/dev/null | wc -l)
lock_guard_count=$(grep -r "std::lock_guard" "${PROJECT_ROOT}/af_core" 2>/dev/null | wc -l)

if [ "$lock_guard_count" -gt 0 ] && [ "$scoped_lock_count" -eq 0 ]; then
    echo "${YELLOW}Warning: Using lock_guard ($lock_guard_count occurrences) but no scoped_lock${NC}"
    echo "  Consider using std::scoped_lock for multiple locks"
else
    echo "${GREEN}✓ Found $scoped_lock_count uses of std::scoped_lock${NC}"
fi

# Method 3: Build with ThreadSanitizer
echo ""
echo "${YELLOW}[3/4] Building with ThreadSanitizer...${NC}"
echo ""

TSAN_BUILD_DIR="${PROJECT_ROOT}/build-tsan"
TSAN_OUTPUT_FILE="/tmp/tsan_build_output.txt"
mkdir -p "$TSAN_BUILD_DIR"

TSAN_STATUS="Not attempted"

if command -v clang++ &> /dev/null; then
    echo "Using clang++ for ThreadSanitizer build..."
    cd "$TSAN_BUILD_DIR"

    # Capture cmake output
    CMAKE_OUTPUT=$(cmake -DCMAKE_CXX_COMPILER=clang++ \
          -DCMAKE_BUILD_TYPE=Debug \
          -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" \
          -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
          "$PROJECT_ROOT" 2>&1)
    CMAKE_EXIT=$?

    echo "$CMAKE_OUTPUT" | tail -20

    if [ $CMAKE_EXIT -eq 0 ]; then
        echo "Building with ThreadSanitizer enabled..."

        # Capture build output
        BUILD_OUTPUT=$(cmake --build . --target af_core -j$(nproc) 2>&1)
        BUILD_EXIT=$?

        echo "$BUILD_OUTPUT" | tail -20

        if [ $BUILD_EXIT -eq 0 ]; then
            echo "${GREEN}✓ Build with ThreadSanitizer successful${NC}"
            echo "  Binary location: ${TSAN_BUILD_DIR}/af_core/af_core"
            echo "  Run tests with: TSAN_OPTIONS='log_path=tsan.log' ./build-tsan/bin/af_core"
            TSAN_STATUS="Success"
        else
            echo "${YELLOW}⚠ Build failed - see output above for details${NC}"
            TSAN_STATUS="Build failed"
            # Save full output to temp file
            echo "$BUILD_OUTPUT" > "$TSAN_OUTPUT_FILE"
        fi
    else
        echo "${YELLOW}⚠ CMake configuration failed - see output above${NC}"
        echo "  Note: TSan build is optional, continuing with static analysis..."
        TSAN_STATUS="CMake configuration failed"
        # Save full output to temp file
        echo "$CMAKE_OUTPUT" > "$TSAN_OUTPUT_FILE"
    fi
else
    echo "${YELLOW}⚠ clang++ not found - skipping ThreadSanitizer build${NC}"
    echo "  Install with: sudo apt-get install clang"
    TSAN_STATUS="clang++ not found"
fi

# Return to project root
cd "$PROJECT_ROOT"

# Method 4: Generate deadlock detection report
echo ""
echo "${YELLOW}[4/4] Generating Lock Analysis Report...${NC}"
echo ""

REPORT_FILE="/tmp/deadlock_analysis_report.txt"
cat > "$REPORT_FILE" << 'EOF'
===============================================
Deadlock Analysis Report
Generated: $(date)
===============================================

1. MUTEX INVENTORY
==================
EOF

# Find all mutex declarations
echo "" >> "$REPORT_FILE"
echo "Mutexes found in codebase:" >> "$REPORT_FILE"
grep -rn "std::mutex\|std::recursive_mutex" "${PROJECT_ROOT}/af_core/include" 2>/dev/null | \
    grep -v "^Binary" | head -20 >> "$REPORT_FILE" || echo "  No mutexes found" >> "$REPORT_FILE"

echo "" >> "$REPORT_FILE"
echo "2. LOCK ACQUISITION SITES" >> "$REPORT_FILE"
echo "=========================" >> "$REPORT_FILE"
grep -rn "lock_guard\|unique_lock\|scoped_lock" "${PROJECT_ROOT}/af_core/src" 2>/dev/null | \
    head -30 >> "$REPORT_FILE" || echo "  No locks found" >> "$REPORT_FILE"

echo "" >> "$REPORT_FILE"
echo "3. THREADSANITIZER BUILD STATUS" >> "$REPORT_FILE"
echo "================================" >> "$REPORT_FILE"
echo "Status: $TSAN_STATUS" >> "$REPORT_FILE"
echo "" >> "$REPORT_FILE"

if [ "$TSAN_STATUS" != "Success" ] && [ "$TSAN_STATUS" != "Not attempted" ] && [ -f "$TSAN_OUTPUT_FILE" ]; then
    echo "ThreadSanitizer Build Output (last 50 lines):" >> "$REPORT_FILE"
    echo "-----------------------------------------------" >> "$REPORT_FILE"
    tail -50 "$TSAN_OUTPUT_FILE" >> "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"
    echo "Full output saved to: $TSAN_OUTPUT_FILE" >> "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"
elif [ "$TSAN_STATUS" = "Success" ]; then
    echo "✓ ThreadSanitizer build completed successfully." >> "$REPORT_FILE"
    echo "  Run with: TSAN_OPTIONS='log_path=tsan.log' ./build-tsan/bin/af_core" >> "$REPORT_FILE"
    echo "" >> "$REPORT_FILE"
fi

echo "" >> "$REPORT_FILE"
echo "5. RECOMMENDATIONS" >> "$REPORT_FILE"
echo "==================" >> "$REPORT_FILE"
cat >> "$REPORT_FILE" << 'RECOMMENDATIONS'

Based on analysis, consider:

✓ Document lock ordering in header files
✓ Use std::scoped_lock when acquiring multiple locks
✓ Avoid calling external methods while holding locks
✓ Run ThreadSanitizer tests regularly
✓ Add lock order assertions in debug builds

Example lock hierarchy documentation:
```cpp
// LOCK ORDERING RULES:
// Level 1 (acquire first): mappings_mutex_
// Level 2: pcf_mapping_mutex_
// Level 3 (acquire last): mtx_
//
// Never acquire locks in reverse order!
// Never hold multiple locks unless using std::scoped_lock
```

RECOMMENDATIONS

echo ""
echo "${GREEN}✓ Report generated: $REPORT_FILE${NC}"
echo ""
cat "$REPORT_FILE"

# Summary
echo ""
echo "==================================="
echo "Summary"
echo "==================================="
echo ""
echo "✓ Static analysis complete"
echo "✓ To catch runtime deadlocks:"
echo "  1. Build with: ./build/scripts/check_deadlocks.sh"
echo "  2. Run tests with ThreadSanitizer enabled"
echo "  3. Check for TSan warnings in output"
echo ""
echo "✓ To prevent deadlocks by design:"
echo "  1. Document lock ordering in code"
echo "  2. Use std::scoped_lock for multiple locks"
echo "  3. Never call external methods while holding locks"
echo ""
echo "${GREEN}Analysis complete!${NC}"

# Exit with error if ThreadSanitizer build failed (for CI pipeline)
if [ "$TSAN_STATUS" != "Success" ] && [ "$TSAN_STATUS" != "Not attempted" ]; then
    echo ""
    echo "${RED}ERROR: ThreadSanitizer build failed - see report for details${NC}"
    exit 1
fi
