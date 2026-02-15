#!/bin/bash
#
# Build script for Spark-optimized JDK
# This builds OpenJDK 25 with G1GC optimizations for Apache Spark workloads
#

set -e  # Exit on error

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$SCRIPT_DIR"

echo "========================================="
echo "Building Spark-Optimized JDK 25"
echo "========================================="
echo ""

# Clean previous build (optional - comment out for incremental builds)
# echo "Cleaning previous build..."
# make clean

# Configure
echo "Configuring build..."
bash configure \
  --with-debug-level=release \
  --with-native-debug-symbols=none \
  --with-jvm-features=compiler2,g1gc,zgc,shenandoahgc \
  --with-extra-cflags="-O3 -march=native" \
  --with-extra-cxxflags="-O3 -march=native" \
  --disable-warnings-as-errors \
  || { echo "Configure failed!"; exit 1; }

echo ""
echo "Configuration complete. Starting build..."
echo ""

# Build
make images CONF=release || { echo "Build failed!"; exit 1; }

echo ""
echo "========================================="
echo "Build Complete!"
echo "========================================="
echo ""
echo "Your Spark-optimized JDK is located at:"
echo "  $(pwd)/build/*/images/jdk"
echo ""
echo "To use it:"
echo "  export JAVA_HOME=$(pwd)/build/*/images/jdk"
echo "  export PATH=\$JAVA_HOME/bin:\$PATH"
echo ""
echo "To enable Spark optimizations when running Spark:"
echo "  spark-submit --conf spark.executor.extraJavaOptions=\"-XX:+UseG1GC -XX:+G1OptimizeForSpark\" your-app.jar"
echo ""
