#!/bin/bash

# Script to test if JVM optimizations actually improve performance
# Compares baseline JDK vs optimized JDK on CPU-bound workloads

set -e

echo "================================="
echo "JVM Optimization Comparison Test"
echo "================================="
echo ""

# Check if Spark is installed
if [ -z "$SPARK_HOME" ]; then
  echo "ERROR: SPARK_HOME not set"
  echo "Please set SPARK_HOME to your Spark installation"
  exit 1
fi

# Check if custom JDK is built
BUILD_DIR="build/linux-x86_64-server-release/jdk"
if [ ! -d "$BUILD_DIR" ]; then
  echo "ERROR: Custom JDK not found at $BUILD_DIR"
  echo "Please build the JDK first: make images"
  exit 1
fi

CUSTOM_JDK="$(pwd)/$BUILD_DIR"

echo "Using Spark: $SPARK_HOME"
echo "Using Custom JDK: $CUSTOM_JDK"
echo ""

# Compile the test
echo "Compiling test..."
$CUSTOM_JDK/bin/javac -cp "$SPARK_HOME/jars/*" SparkOptimizationTest.scala
echo ""

# Run with BASELINE (optimizations disabled)
echo "================================="
echo "BASELINE: Standard G1GC (No Optimizations)"
echo "================================="
JAVA_HOME=$CUSTOM_JDK $SPARK_HOME/bin/spark-submit \
  --master local[4] \
  --driver-memory 4g \
  --class SparkOptimizationTest \
  --conf "spark.driver.extraJavaOptions=-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions -XX:-G1OptimizeForSpark -Xlog:gc:file=/tmp/gc-baseline.log" \
  --conf "spark.executor.extraJavaOptions=-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions -XX:-G1OptimizeForSpark" \
  SparkOptimizationTest.scala

echo ""
echo ""

# Run with OPTIMIZATIONS ENABLED
echo "================================="
echo "OPTIMIZED: Spark-Optimized JDK"
echo "================================="
JAVA_HOME=$CUSTOM_JDK $SPARK_HOME/bin/spark-submit \
  --master local[4] \
  --driver-memory 4g \
  --class SparkOptimizationTest \
  --conf "spark.driver.extraJavaOptions=-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -Xlog:gc:file=/tmp/gc-optimized.log" \
  --conf "spark.executor.extraJavaOptions=-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations" \
  SparkOptimizationTest.scala

echo ""
echo ""

# Analyze GC logs
echo "================================="
echo "GC Analysis"
echo "================================="
echo ""
echo "Baseline GC:"
grep -i "pause" /tmp/gc-baseline.log | tail -5 || echo "No GC pauses found"
echo ""
echo "Optimized GC:"
grep -i "pause" /tmp/gc-optimized.log | tail -5 || echo "No GC pauses found"

echo ""
echo "Test complete. Compare the times above to see if optimizations help."
echo "If times are similar, your workload may be I/O bound, not CPU/GC bound."
