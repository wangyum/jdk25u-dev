#!/bin/bash

# Local test script for macOS

set -e

echo "================================="
echo "JVM Optimization Test - Local"
echo "================================="
echo ""

# Find the JDK build
JDK_BUILD=$(find build -type d -name "jdk" -path "*/macosx-*" | grep -v "interim" | grep -v "support" | head -1)

if [ -z "$JDK_BUILD" ]; then
    echo "ERROR: No macOS JDK build found"
    echo "Please build the JDK first:"
    echo "  bash configure && make images"
    exit 1
fi

echo "Using JDK: $JDK_BUILD"
echo ""

# Check if JDK has java executable
if [ ! -f "$JDK_BUILD/bin/java" ]; then
    echo "ERROR: java executable not found in $JDK_BUILD/bin/"
    exit 1
fi

# Show JDK version
echo "JDK Version:"
$JDK_BUILD/bin/java -version
echo ""

# Compile the test
echo "Compiling test..."
$JDK_BUILD/bin/javac JVMOptimizationTest.java
echo ""

# Run BASELINE (optimizations disabled)
echo "================================="
echo "BASELINE: Standard G1GC"
echo "================================="
$JDK_BUILD/bin/java \
    -Xms2g -Xmx2g \
    -XX:+UseG1GC \
    -XX:+UnlockExperimentalVMOptions \
    -XX:-G1OptimizeForSpark \
    -Xlog:gc:file=/tmp/gc-baseline.log \
    JVMOptimizationTest

echo ""
echo ""

# Run OPTIMIZED (all optimizations enabled)
echo "================================="
echo "OPTIMIZED: Spark-Optimized JDK"
echo "================================="
$JDK_BUILD/bin/java \
    -Xms2g -Xmx2g \
    -XX:+UseG1GC \
    -XX:+UnlockExperimentalVMOptions \
    -XX:+G1OptimizeForSpark \
    -XX:+G1SparkEnhanceEscapeAnalysis \
    -XX:+G1SparkOptimizeHashOperations \
    -XX:+G1SparkEnableVectorization \
    -Xlog:gc:file=/tmp/gc-optimized.log \
    JVMOptimizationTest

echo ""
echo ""

# GC Analysis
echo "================================="
echo "GC Analysis"
echo "================================="

if [ -f /tmp/gc-baseline.log ]; then
    echo ""
    echo "Baseline GC summary:"
    BASELINE_PAUSES=$(grep -c "Pause" /tmp/gc-baseline.log || echo "0")
    echo "  Total GC pauses: $BASELINE_PAUSES"

    if [ "$BASELINE_PAUSES" -gt "0" ]; then
        echo "  Last 3 pauses:"
        grep "Pause" /tmp/gc-baseline.log | tail -3
    fi
fi

if [ -f /tmp/gc-optimized.log ]; then
    echo ""
    echo "Optimized GC summary:"
    OPTIMIZED_PAUSES=$(grep -c "Pause" /tmp/gc-optimized.log || echo "0")
    echo "  Total GC pauses: $OPTIMIZED_PAUSES"

    if [ "$OPTIMIZED_PAUSES" -gt "0" ]; then
        echo "  Last 3 pauses:"
        grep "Pause" /tmp/gc-optimized.log | tail -3
    fi
fi

echo ""
echo "================================="
echo "Test Complete!"
echo "================================="
echo ""
echo "Compare the times above to see optimization impact."
echo ""
echo "If you see 20-40% improvement:"
echo "  ✅ Your JVM optimizations are working!"
echo ""
echo "If times are similar (<5% difference):"
echo "  - Check if optimizations are actually enabled"
echo "  - Your workload may not benefit from these specific optimizations"
echo ""
echo "GC logs saved to:"
echo "  Baseline: /tmp/gc-baseline.log"
echo "  Optimized: /tmp/gc-optimized.log"
