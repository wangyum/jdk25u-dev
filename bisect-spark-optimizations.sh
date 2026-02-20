#!/bin/bash

# Bisection test script to identify which Spark optimization flag causes regression
# Focus on q3 which shows -43.9% regression

set -e

BRANCH="spark"
QUERY="q3"

echo "============================================================"
echo "Spark Optimization Flags Bisection Test"
echo "============================================================"
echo "Testing query: $QUERY (baseline: 426ms, regressed to 613ms = -43.9%)"
echo ""

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test function
run_test() {
    local test_name="$1"
    local opts="$2"

    echo ""
    echo "============================================================"
    echo "Test: $test_name"
    echo "============================================================"
    echo "Options: $opts"
    echo ""

    gh workflow run spark-benchmark.yml \
      --ref "$BRANCH" \
      -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
      -f optimized_opts="$opts" \
      -f query_filter="$QUERY" \
      -f log_level="ERROR"

    echo "✅ Test '$test_name' triggered"
    echo "   Check results at: https://github.com/wangyum/jdk25u-dev/actions"
}

echo "Starting bisection tests..."
echo ""

# Baseline - no optimizations (should be ~426ms)
run_test "0-BASELINE" \
  "-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch"

sleep 2

# Test 1: Only G1OptimizeForSpark (master switch)
run_test "1-G1OptimizeForSpark-ONLY" \
  "-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:-G1SparkEnhanceEscapeAnalysis -XX:-G1SparkOptimizeHashOperations -XX:-G1SparkEnableVectorization"

sleep 2

# Test 2: G1OptimizeForSpark + Escape Analysis
run_test "2-WITH-EscapeAnalysis" \
  "-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:-G1SparkOptimizeHashOperations -XX:-G1SparkEnableVectorization"

sleep 2

# Test 3: G1OptimizeForSpark + Hash Operations
run_test "3-WITH-HashOperations" \
  "-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:-G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:-G1SparkEnableVectorization"

sleep 2

# Test 4: G1OptimizeForSpark + Vectorization
run_test "4-WITH-Vectorization" \
  "-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:-G1SparkEnhanceEscapeAnalysis -XX:-G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization"

sleep 2

# Test 5: All optimizations (should show regression ~613ms)
run_test "5-ALL-OPTIMIZATIONS" \
  "-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization"

echo ""
echo "============================================================"
echo "All Tests Triggered!"
echo "============================================================"
echo ""
echo "Results will be available at:"
echo "https://github.com/wangyum/jdk25u-dev/actions"
echo ""
echo "Expected results:"
echo "  Test 0 (baseline):     ~426ms  (reference)"
echo "  Test 5 (all opts):     ~613ms  (known regression)"
echo "  Tests 1-4:             TBD - identifies which flag causes regression"
echo ""
echo "Analysis:"
echo "  - If Test 1 shows regression → G1OptimizeForSpark itself is the problem"
echo "  - If Test 2 shows regression → Escape Analysis causes it"
echo "  - If Test 3 shows regression → Hash Operations causes it"
echo "  - If Test 4 shows regression → Vectorization causes it"
echo ""
echo "Check workflow runs with:"
echo "  gh run list --workflow=spark-benchmark.yml --limit=10"
echo ""
