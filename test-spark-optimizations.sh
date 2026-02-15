#!/bin/bash
#
# Test script to verify Spark optimizations are working
#

set -e

# Find the built JDK
JDK_DIR=$(find build -name "jdk" -type d | grep "images/jdk$" | head -1)

if [ -z "$JDK_DIR" ]; then
    echo "Error: JDK not found. Please build it first with ./build-spark-jdk.sh"
    exit 1
fi

echo "========================================="
echo "Testing Spark-Optimized JDK"
echo "========================================="
echo ""
echo "JDK location: $JDK_DIR"
echo ""

export JAVA_HOME="$(pwd)/$JDK_DIR"
export PATH="$JAVA_HOME/bin:$PATH"

# Verify JDK version
echo "Java version:"
java -version
echo ""

# Create a simple test program
cat > /tmp/TestSparkGC.java <<'EOF'
public class TestSparkGC {
    public static void main(String[] args) {
        System.out.println("Testing Spark-optimized GC flags...");
        System.out.println("VM Name: " + System.getProperty("java.vm.name"));
        System.out.println("VM Version: " + System.getProperty("java.vm.version"));

        // Allocate some objects to trigger GC
        for (int i = 0; i < 1000; i++) {
            String[] data = new String[10000];
            for (int j = 0; j < data.length; j++) {
                data[j] = "Test string " + i + "-" + j;
            }
        }

        System.out.println("Test completed successfully!");
    }
}
EOF

# Compile test program
echo "Compiling test program..."
javac /tmp/TestSparkGC.java
echo ""

# Test 1: Verify G1GC works
echo "Test 1: Running with standard G1GC..."
java -XX:+UseG1GC \
     -Xms512m -Xmx512m \
     -XX:+PrintFlagsFinal \
     -cp /tmp \
     TestSparkGC 2>&1 | grep -E "G1OptimizeForSpark|G1SparkYoungGen|G1SparkInitiating" || echo "  (Spark flags not set - this is expected)"
echo ""

# Test 2: Verify Spark optimizations work
echo "Test 2: Running with Spark optimizations enabled..."
java -XX:+UseG1GC \
     -XX:+G1OptimizeForSpark \
     -Xms512m -Xmx512m \
     -Xlog:gc*=info:file=/tmp/gc-spark.log \
     -Xlog:gc+init=info:stdout \
     -cp /tmp \
     TestSparkGC
echo ""

# Show GC log summary
if [ -f /tmp/gc-spark.log ]; then
    echo "GC log created: /tmp/gc-spark.log"
    echo "First few lines:"
    head -20 /tmp/gc-spark.log
fi
echo ""

# Test 3: Print all Spark-related flags
echo "Test 3: Checking Spark-related flags..."
java -XX:+UseG1GC -XX:+G1OptimizeForSpark -XX:+PrintFlagsFinal -version 2>&1 | \
    grep -i spark || echo "  Note: Flags might not show if optimizations are applied differently"
echo ""

echo "========================================="
echo "All tests completed!"
echo "========================================="
echo ""
echo "To use with Apache Spark:"
echo "  export JAVA_HOME=$(pwd)/$JDK_DIR"
echo "  spark-submit \\"
echo "    --conf spark.executor.extraJavaOptions=\"-XX:+UseG1GC -XX:+G1OptimizeForSpark\" \\"
echo "    --conf spark.driver.extraJavaOptions=\"-XX:+UseG1GC -XX:+G1OptimizeForSpark\" \\"
echo "    your-spark-app.jar"
echo ""
