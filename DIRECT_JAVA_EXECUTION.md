# Direct Java Execution for Fair Benchmark Comparison

## Problem Identified

When running benchmarks in parallel with `./build/sbt "sql/Test/runMain ..."`:
- ❌ Both processes invoke SBT
- ❌ SBT checks for compilation (even if already compiled)
- ❌ Resource contention during SBT startup
- ❌ Not a fair comparison (different SBT overhead)
- ❌ Slower overall execution

## Solution

**Compile once, run directly with `java -cp`:**

### Step 1: Compile Once
```bash
./build/sbt "sql/Test/compile"          # Compile test classes
./build/sbt "sql/Test/exportClasspath"  # Get classpath
```

### Step 2: Run Both Benchmarks Directly (No SBT)
```bash
# BASELINE - Direct java execution
java -Xmx4g -XX:+UseG1GC \
  -cp "$CLASSPATH" \
  org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  --data-location $DATA --query-filter "q3,q7,..." &

# OPTIMIZED - Direct java execution
java -Xmx4g -XX:+UseG1GC -XX:+G1OptimizeForSpark \
  -cp "$CLASSPATH" \
  org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  --data-location $DATA --query-filter "q3,q7,..." &

# Wait for both
wait
```

## Benefits

### 1. No Compilation Contention ✅
- Compile once upfront
- Both benchmarks use same compiled classes
- No incremental compilation checks during execution

### 2. No SBT Overhead ✅
- Direct JVM startup
- No SBT daemon/server overhead
- Faster startup time

### 3. Fair Comparison ✅
- Both run exactly the same way
- Same JVM startup process
- Only difference is JVM flags (baseline vs optimized)

### 4. True Parallel Execution ✅
- Both start nearly simultaneously
- No waiting for SBT to initialize
- Maximum CPU utilization

### 5. Cleaner Logs ✅
- No SBT output mixed in
- Pure benchmark output
- Easier to parse timing data

## Before vs After

### Before (SBT-based)
```bash
# Both processes do:
./build/sbt "sql/Test/runMain Benchmark ..." &
./build/sbt "sql/Test/runMain Benchmark ..." &

# Each process:
1. Starts SBT JVM
2. Checks for compilation (incremental compile check)
3. Maybe recompiles if dependencies changed
4. Starts benchmark JVM
5. Runs benchmark
```

**Issues:**
- Two SBT JVMs running
- Possible compilation contention
- Unpredictable timing

### After (Direct Java)
```bash
# Pre-compile once
./build/sbt "sql/Test/compile"
./build/sbt "sql/Test/exportClasspath" > classpath.txt

# Both processes do:
java -cp "$CLASSPATH" Benchmark ... &
java -cp "$CLASSPATH" Benchmark ... &

# Each process:
1. Starts benchmark JVM directly
2. Runs benchmark
```

**Benefits:**
- No SBT overhead
- Guaranteed no recompilation
- Predictable timing

## Implementation Details

### Classpath Export

```bash
./build/sbt "sql/Test/exportClasspath" > /tmp/classpath.txt
CLASSPATH=$(tail -1 /tmp/classpath.txt)
```

This exports the full classpath including:
- Compiled test classes
- Spark jars
- All dependencies
- Test resources

### Direct Execution

```bash
$JAVA_HOME/bin/java \
  -Xmx4g \
  -XX:+UseG1GC \
  -cp "$BENCHMARK_CLASSPATH" \
  org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3,q7,..."
```

**Key points:**
- Uses custom JDK (`$JAVA_HOME/bin/java`)
- Direct classpath specification
- No SBT wrapper
- Clean benchmark execution

## Performance Impact

### Compilation Phase
- **Before:** ~5 min × 2 = 10 min (both processes compile)
- **After:** ~5 min × 1 = 5 min (compile once)
- **Savings:** 5 minutes

### Startup Time
- **Before:** SBT startup ~10 sec × 2 = 20 sec
- **After:** Direct java startup ~2 sec × 2 = 4 sec
- **Savings:** 16 seconds

### Benchmark Execution
- **Before:** 30 min (with possible SBT interference)
- **After:** 30 min (clean execution)
- **Improvement:** More consistent, fair comparison

### Total Time Saved
- ~5-6 minutes per workflow run
- More reliable results
- Better resource utilization

## Workflow Steps

```yaml
- name: Clone and build Apache Spark
  run: ./build/sbt package

- name: Compile Spark SQL module and prepare classpath
  run: |
    ./build/sbt "sql/Test/compile"
    ./build/sbt "sql/Test/exportClasspath" > classpath.txt
    BENCHMARK_CLASSPATH=$(tail -1 classpath.txt)

- name: Run benchmarks in parallel
  run: |
    # BASELINE
    java -cp "$BENCHMARK_CLASSPATH" ... &

    # OPTIMIZED
    java -cp "$BENCHMARK_CLASSPATH" ... &

    wait
```

## Error Handling

If classpath export fails:
```bash
./build/sbt "sql/Test/exportClasspath" > /tmp/classpath.txt

if [ ! -s /tmp/classpath.txt ]; then
  echo "ERROR: Failed to export classpath"
  exit 1
fi
```

If benchmark fails:
```bash
wait $BASELINE_PID
BASELINE_EXIT=$?

if [ $BASELINE_EXIT -ne 0 ]; then
  echo "ERROR: BASELINE benchmark failed!"
  cat benchmark-baseline.log
  exit 1
fi
```

## Comparison with Other Approaches

### Approach 1: Sequential SBT (Original)
```bash
./build/sbt "sql/Test/runMain Benchmark ..."  # Run 1
./build/sbt "sql/Test/runMain Benchmark ..."  # Run 2
```
- ❌ Sequential (slow)
- ❌ SBT overhead × 2
- ✅ Simple

### Approach 2: Parallel SBT (First improvement)
```bash
./build/sbt "sql/Test/runMain Benchmark ..." &  # Run 1
./build/sbt "sql/Test/runMain Benchmark ..." &  # Run 2
wait
```
- ✅ Parallel
- ❌ SBT overhead × 2
- ❌ Compilation contention
- ⚠️ Not truly fair comparison

### Approach 3: Direct Java (Current - Best)
```bash
./build/sbt "sql/Test/compile"  # Compile once
java -cp ... Benchmark ... &    # Run 1
java -cp ... Benchmark ... &    # Run 2
wait
```
- ✅ Parallel
- ✅ No SBT overhead
- ✅ No compilation contention
- ✅ Fair comparison
- ✅ Fastest

## Verification

To verify both benchmarks run identically:

```bash
# Check both use same classpath
echo "BASELINE classpath: $BENCHMARK_CLASSPATH"
echo "OPTIMIZED classpath: $BENCHMARK_CLASSPATH"

# Check both use same JDK
echo "BASELINE JDK: $JAVA_HOME"
echo "OPTIMIZED JDK: $JAVA_HOME"

# Only JVM flags should differ
diff <(ps aux | grep BASELINE | grep -o -- '-XX:[^ ]*') \
     <(ps aux | grep OPTIMIZED | grep -o -- '-XX:[^ ]*')
```

Expected diff:
```
< -XX:+UseG1GC
---
> -XX:+UseG1GC
> -XX:+UnlockExperimentalVMOptions
> -XX:+G1OptimizeForSpark
> -XX:+G1SparkEnhanceEscapeAnalysis
> ...
```

## Summary

**Before:** SBT wrapper → compile check → maybe recompile → run
**After:** Compile once → run directly (×2 in parallel)

**Result:**
- ✅ Faster execution
- ✅ Fair comparison
- ✅ No compilation contention
- ✅ Cleaner logs
- ✅ More reliable results
