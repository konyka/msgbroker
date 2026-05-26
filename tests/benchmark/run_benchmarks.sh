#!/bin/bash
set -euo pipefail

BUILD_DIR="${1:-build-release}"
BASELINE_FILE="${2:-tests/benchmark/baseline.txt}"
THRESHOLD_PCT="${3:-20}"

echo "=== msgbroker benchmark regression CI ==="
echo "build: $BUILD_DIR"
echo "baseline: $BASELINE_FILE"
echo "threshold: ${THRESHOLD_PCT}%"
echo ""

if [ ! -f "$BASELINE_FILE" ]; then
    echo "BASELINE MISSING: $BASELINE_FILE"
    echo "Creating baseline from current build..."
    mkdir -p "$(dirname "$BASELINE_FILE")"

    result=$("$BUILD_DIR/perf/inproc_thr" 2>/dev/null | grep "msg/sec" | awk '{print $2}')
    echo "inproc_throughput=$result" > "$BASELINE_FILE"
    echo "Baseline created: inproc_throughput=$result"
    exit 0
fi

source "$BASELINE_FILE"

current=$("$BUILD_DIR/perf/inproc_thr" 2>/dev/null | grep "msg/sec" | awk '{print $2}')

echo "baseline: ${inproc_throughput:-N/A} msg/sec"
echo "current:  $current msg/sec"

if [ -z "${inproc_throughput:-}" ] || [ -z "$current" ]; then
    echo "ERROR: Could not read benchmark values"
    exit 1
fi

pct_diff=$(echo "scale=1; ($inproc_throughput - $current) * 100 / $inproc_throughput" | bc)

echo "diff: ${pct_diff}%"

if (( $(echo "$pct_diff > $THRESHOLD_PCT" | bc -l) )); then
    echo "FAIL: Performance regression > ${THRESHOLD_PCT}%"
    echo "  baseline: $inproc_throughput msg/sec"
    echo "  current:  $current msg/sec"
    echo "  diff:     ${pct_diff}%"
    exit 1
fi

echo "PASS: No regression (within ${THRESHOLD_PCT}%)"
