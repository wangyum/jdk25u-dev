#!/bin/bash

# Test a single Spark optimization flag

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 <flag-name> [query]"
    echo ""
    echo "Available flags:"
    echo "  G1OptimizeForSpark"
    echo "  G1SparkEnhanceEscapeAnalysis"
    echo "  G1SparkOptimizeHashOperations"
    echo "  G1SparkEnableVectorization"
    echo ""
    echo "Examples:"
    echo "  $0 G1OptimizeForSpark q3"
    echo "  $0 G1SparkEnhanceEscapeAnalysis"
    exit 1
fi

FLAG="$1"
QUERY="${2:-q3}"
BRANCH="spark"

echo "============================================================"
echo "Testing Single Flag: $FLAG"
echo "============================================================"
echo "Query: $QUERY"
echo ""

# Build options based on flag
case "$FLAG" in
    "G1OptimizeForSpark")
        OPTS="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:-G1SparkEnhanceEscapeAnalysis -XX:-G1SparkOptimizeHashOperations -XX:-G1SparkEnableVectorization"
        ;;
    "G1SparkEnhanceEscapeAnalysis")
        OPTS="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:-G1SparkOptimizeHashOperations -XX:-G1SparkEnableVectorization"
        ;;
    "G1SparkOptimizeHashOperations")
        OPTS="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:-G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:-G1SparkEnableVectorization"
        ;;
    "G1SparkEnableVectorization")
        OPTS="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:-G1SparkEnhanceEscapeAnalysis -XX:-G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization"
        ;;
    *)
        echo "Error: Unknown flag '$FLAG'"
        exit 1
        ;;
esac

echo "Testing with options:"
echo "$OPTS"
echo ""

gh workflow run spark-benchmark.yml \
  --ref "$BRANCH" \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="$OPTS" \
  -f query_filter="$QUERY" \
  -f log_level="ERROR"

echo ""
echo "✅ Test triggered for flag: $FLAG"
echo ""
echo "Check results at:"
echo "  gh run list --workflow=spark-benchmark.yml --limit=5"
echo ""
echo "Expected baseline time for q3: ~426ms"
echo "Looking to see if this flag causes the -43.9% regression to 613ms"
echo ""
