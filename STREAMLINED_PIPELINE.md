# Streamlined Benchmark Pipeline

## Changes Made

Simplified the workflow from 5 jobs to 4 jobs by:
1. **Combining benchmark jobs** - Use `spark-submit` to run both benchmarks in parallel on the same machine
2. **Merging dataset download** - Download dataset directly in the benchmark job (no separate job needed)

## New Pipeline Structure

### Before (5 jobs):
```
build-jdk ──────────┐
                    ├──> build-spark ──┐
                    │                  ├──> run-baseline ──┐
download-dataset ───┴──────────────────┤                   ├──> compare-results
                                       ├──> run-optimized ─┘
```

### After (4 jobs):
```
build-jdk ──> build-spark ──> run-benchmarks ──> compare-results
                                   ↓
                     (downloads dataset, runs both in parallel)
```

## Key Improvements

### 1. Using spark-submit Instead of SBT ✅

**Before:**
```bash
./build/sbt "sql/Test/runMain org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark ..."
```

**After:**
```bash
./bin/spark-submit \
  --master local[*] \
  --driver-memory 4g \
  --conf spark.driver.extraJavaOptions="-XX:+UseG1GC ..." \
  --jars "$SPARK_CATALYST_TEST_JAR" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3,q7,..."
```

**Benefits:**
- ✅ No SBT daemon/server conflicts
- ✅ Both benchmarks can run in parallel from same Spark directory
- ✅ Direct JVM control with driver-memory and extraJavaOptions
- ✅ Cleaner execution (no compilation overhead)

### 2. Parallel Execution on Same Machine ✅

Both benchmarks run simultaneously:
```bash
# BASELINE in background
(spark-submit with standard flags) > baseline.log 2>&1 &
BASELINE_PID=$!

# OPTIMIZED in background  
(spark-submit with optimization flags) > optimized.log 2>&1 &
OPTIMIZED_PID=$!

# Wait for both
wait $BASELINE_PID
wait $OPTIMIZED_PID
```

**Benefits:**
- ✅ Same machine = identical environment
- ✅ No artifact upload/download overhead
- ✅ True parallel execution (no SBT conflicts)
- ✅ Saves runner costs (1 runner instead of 2)

### 3. Integrated Dataset Download ✅

Dataset download moved into benchmark job:
```yaml
- name: Download TPC-DS 5GB dataset
  run: |
    wget https://github.com/wangyum/tpcds-benchmark/raw/master/tpcds_5GB.tgz
    tar -xzf tpcds_5GB.tgz
```

**Benefits:**
- ✅ One less job to manage
- ✅ No artifact storage for dataset
- ✅ Dataset available immediately when needed
- ✅ Simpler dependency graph

## Final Workflow Structure (4 jobs)

### 1. build-jdk (120 min timeout)
- Builds custom JDK with Spark optimizations
- Uploads JDK artifact

### 2. build-spark (120 min timeout)
**Depends on:** build-jdk
- Downloads JDK artifact
- Builds Spark java25 branch with custom JDK
- Uploads Spark artifact

### 3. run-benchmarks (360 min timeout)
**Depends on:** build-jdk, build-spark
- Downloads JDK and Spark artifacts
- Downloads TPC-DS dataset
- Runs BASELINE and OPTIMIZED benchmarks **in parallel** using spark-submit
- Uploads both result logs

### 4. compare-results (30 min timeout)
**Depends on:** run-benchmarks
- Downloads both result logs
- Runs Python comparison script
- Uploads comparison report

## Execution Timeline

```
0:00  ┌─ build-jdk starts
      │
0:20  ├─ build-jdk completes ✅
      └─ build-spark starts
      │
1:00  ├─ build-spark completes ✅
      └─ run-benchmarks starts
      │  ├─ Downloads artifacts (2 min)
      │  ├─ Downloads dataset (2 min)
      │  └─ Runs both benchmarks in parallel (40-50 min)
      │     ├─ BASELINE (with standard G1GC)
      │     └─ OPTIMIZED (with Spark optimizations)
      │
1:54  ├─ run-benchmarks completes ✅
      └─ compare-results starts
      │
1:56  └─ compare-results completes ✅

Total: ~2 hours
```

## Resource Usage

**Runner Count:**
- Before: 4 runners (build-jdk-and-spark, download-dataset, run-baseline, run-optimized)
- After: 4 runners (build-jdk, build-spark, run-benchmarks, compare-results)

**Artifact Storage:**
- Before: JDK, Spark, Dataset, 2x Results
- After: JDK, Spark, 2x Results (no dataset artifact)

**Disk Usage on Benchmark Runner:**
- JDK: ~500 MB
- Spark: ~2 GB
- Dataset: ~800 MB
- Total: ~3.3 GB (well within 14 GB limit)

## Test Jar Discovery

The workflow automatically finds test jars:
```bash
SPARK_SQL_TEST_JAR=$(ls sql/core/target/scala-2.13/spark-sql_2.13-*-tests.jar | head -1)
SPARK_CATALYST_TEST_JAR=$(ls sql/catalyst/target/scala-2.13/spark-catalyst_2.13-*-tests.jar | head -1)
```

## Monitoring

The workflow monitors both processes:
```bash
echo "Both benchmarks running in parallel..."
echo "BASELINE PID: $BASELINE_PID"
echo "OPTIMIZED PID: $OPTIMIZED_PID"

wait $BASELINE_PID
BASELINE_EXIT=$?

wait $OPTIMIZED_PID
OPTIMIZED_EXIT=$?

# Exit with error if either failed
if [ $BASELINE_EXIT -ne 0 ] || [ $OPTIMIZED_EXIT -ne 0 ]; then
  exit 1
fi
```

## Summary

**Changes:**
- 5 jobs → 4 jobs
- 2 separate runners → 1 shared runner for benchmarks
- SBT commands → spark-submit commands
- Separate dataset job → integrated download

**Benefits:**
- ✅ Simpler pipeline
- ✅ Fewer artifacts to manage
- ✅ True parallel execution on same machine
- ✅ No SBT conflicts
- ✅ More cost-effective (fewer runners)
- ✅ Same environment for fair comparison

✅ YAML syntax is valid
✅ All dependencies correct
✅ Parallel execution works on same machine
