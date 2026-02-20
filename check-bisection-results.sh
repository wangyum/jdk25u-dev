#!/bin/bash

# Check bisection test results

echo "============================================================"
echo "Bisection Test Results Checker"
echo "============================================================"
echo ""

# Get recent runs
echo "Recent workflow runs:"
echo "------------------------------------------------------------"
gh run list --workflow=spark-benchmark.yml --limit=10

echo ""
echo "============================================================"
echo "Checking for completed runs..."
echo "============================================================"
echo ""

# Count completed vs in-progress
TOTAL=$(gh run list --workflow=spark-benchmark.yml --limit=10 --json status | jq '. | length')
COMPLETED=$(gh run list --workflow=spark-benchmark.yml --limit=10 --json status | jq '[.[] | select(.status=="completed")] | length')
IN_PROGRESS=$(gh run list --workflow=spark-benchmark.yml --limit=10 --json status | jq '[.[] | select(.status=="in_progress" or .status=="queued")] | length')

echo "Status:"
echo "  Total runs: $TOTAL"
echo "  Completed: $COMPLETED"
echo "  In Progress: $IN_PROGRESS"
echo ""

if [ "$COMPLETED" -ge 6 ]; then
    echo "✅ Bisection tests complete! (at least 6 runs finished)"
    echo ""
    echo "To download results:"
    echo "------------------------------------------------------------"
    gh run list --workflow=spark-benchmark.yml --limit=6 --json databaseId,status,conclusion,startedAt | \
        jq -r '.[] | "  gh run download \(.databaseId)  # Started: \(.startedAt) - \(.conclusion)"'
    echo ""
    echo "To view a specific run:"
    echo "  gh run view <run-id> --log"
    echo ""
    echo "To analyze q3 times, grep the benchmark logs:"
    echo "  grep -A5 'q3' benchmark-*.log"
    echo ""
elif [ "$IN_PROGRESS" -gt 0 ]; then
    echo "⏳ Tests still running..."
    echo ""
    echo "Expected time remaining: ~$((IN_PROGRESS * 30)) minutes (30min per test)"
    echo ""
    echo "Watch a running test:"
    RUN_ID=$(gh run list --workflow=spark-benchmark.yml --limit=1 --json databaseId,status | jq -r '.[] | select(.status=="in_progress") | .databaseId')
    if [ -n "$RUN_ID" ]; then
        echo "  gh run watch $RUN_ID"
    fi
    echo ""
    echo "Re-run this script to check again:"
    echo "  ./check-bisection-results.sh"
else
    echo "⚠️ No in-progress runs found"
    echo ""
    echo "Check if tests failed:"
    gh run list --workflow=spark-benchmark.yml --limit=6 --json status,conclusion | \
        jq -r '.[] | select(.conclusion=="failure") | "FAILED: \(.)"'
fi

echo ""
echo "============================================================"
echo "Quick Analysis Guide"
echo "============================================================"
echo ""
echo "Once tests complete, look for q3 times in benchmark logs:"
echo ""
echo "Test 0 (baseline):         Expected ~426ms"
echo "Test 1 (G1Optimize only):  Expected ~500-550ms (G1 tuning issue)"
echo "Test 2 (+ EscapeAnalysis): Expected ~580-630ms (EA adds overhead)"
echo "Test 3 (+ HashOps):        Expected ~430-450ms (minimal change)"
echo "Test 4 (+ Vectorization):  Expected ~430-450ms (minimal change)"
echo "Test 5 (all opts):         Expected ~613ms (confirmed regression)"
echo ""
echo "The test(s) showing significant regression indicate the problematic flag(s)."
echo ""
