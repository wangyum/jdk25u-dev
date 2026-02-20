#!/bin/bash

# Run Spark benchmark with diagnostic logging enabled

set -e

BRANCH="spark"
QUERY="${1:-q3}"  # Default to q3 if not specified

echo "============================================================"
echo "Spark Benchmark with Diagnostic Logging"
echo "============================================================"
echo "Query: $QUERY"
echo ""

# Test with all diagnostics enabled
DIAGNOSTIC_OPTS="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
-XX:+UnlockExperimentalVMOptions \
-XX:+UnlockDiagnosticVMOptions \
-XX:+G1OptimizeForSpark \
-XX:+G1SparkEnhanceEscapeAnalysis \
-XX:+G1SparkOptimizeHashOperations \
-XX:+G1SparkEnableVectorization \
-Xlog:gc*=info:file=/tmp/gc-optimized.log \
-XX:+PrintCompilation \
-XX:+LogCompilation \
-XX:LogFile=/tmp/compilation-optimized.log \
-XX:+TraceDeoptimization \
-XX:+PrintDeoptimizationDetails"

BASELINE_OPTS="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
-XX:+UnlockDiagnosticVMOptions \
-Xlog:gc*=info:file=/tmp/gc-baseline.log \
-XX:+PrintCompilation \
-XX:+LogCompilation \
-XX:LogFile=/tmp/compilation-baseline.log"

echo "Running benchmark with full diagnostics..."
echo ""
echo "This will generate log files:"
echo "  /tmp/gc-baseline.log           - GC activity for baseline"
echo "  /tmp/gc-optimized.log          - GC activity for optimized"
echo "  /tmp/compilation-baseline.log  - JIT compilation for baseline"
echo "  /tmp/compilation-optimized.log - JIT compilation for optimized"
echo ""

gh workflow run spark-benchmark.yml \
  --ref "$BRANCH" \
  -f baseline_opts="$BASELINE_OPTS" \
  -f optimized_opts="$DIAGNOSTIC_OPTS" \
  -f query_filter="$QUERY" \
  -f log_level="WARN"

echo ""
echo "✅ Diagnostic run triggered!"
echo ""
echo "After the run completes, analyze the logs to find:"
echo ""
echo "1. GC Differences:"
echo "   - Number of GC cycles"
echo "   - GC pause times"
echo "   - Memory allocation patterns"
echo ""
echo "2. Compilation Differences:"
echo "   - Number of methods compiled"
echo "   - Deoptimization events"
echo "   - Optimization decisions"
echo ""
echo "3. Look for:"
echo "   - 'Uncommon trap' messages (deoptimization)"
echo "   - 'made not entrant' (code invalidated)"
echo "   - 'made zombie' (code removed)"
echo ""
echo "Check workflow status:"
echo "  gh run list --workflow=spark-benchmark.yml --limit=5"
echo ""
