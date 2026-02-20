# Spark Benchmark Workflow Review

## Overall Assessment: ✅ Good with Critical Issues

The workflow implements custom JDK optimization comparison but has several critical validation gaps.

## Workflow Summary

**Purpose:** Compare custom JDK 25 optimizations vs baseline using TPC-DS benchmarks on Spark

**Structure:** Single job with 13 steps

**Runtime:** ~130 minutes (2h JDK build + 30min Spark + 40min benchmarks)

**Triggers:**
- Push to `main` or `spark` branches (with path filters)
- Pull requests to `main`
- Manual workflow dispatch (with scale options)

## Step-by-Step Review

### 1. Checkout JDK code ✅
```yaml
- uses: actions/checkout@v4
  with:
    fetch-depth: 1
```

**Status:** ✅ Correct
- Shallow clone for speed
- Fetches JDK source code

### 2. Install JDK build dependencies ✅
```yaml
sudo apt-get install -y \
  build-essential autoconf zip unzip \
  libx11-dev libxext-dev libxrender-dev \
  ...
```

**Status:** ✅ Comprehensive
- All required JDK build dependencies
- Includes ccache for faster rebuilds

### 3. Install Bootstrap JDK ✅
```yaml
- uses: actions/setup-java@v4
  with:
    distribution: 'temurin'
    java-version: '21'
```

**Status:** ✅ Correct
- JDK 21 needed to build JDK 25
- Uses Temurin distribution

### 4. Set Boot JDK path ✅
```yaml
BOOT_JDK=$(find /opt/hostedtoolcache/Java_Temurin-Hotspot_jdk -maxdepth 2 -type d -name "x64" | head -1)
```

**Status:** ✅ Good
- Finds actual boot JDK path
- Verifies with `java -version`

### 5. Configure JDK build ✅
```yaml
bash configure \
  --with-boot-jdk=$BOOT_JDK \
  --with-native-debug-symbols=none \
  --with-debug-level=release \
  --with-version-opt="spark-optimized" \
  --disable-warnings-as-errors
```

**Status:** ✅ Correct
- Release build (optimized)
- No debug symbols (faster build)
- Custom version tag

### 6. Build JDK ✅
```yaml
make images
echo "JDK_HOME=$(pwd)/build/linux-x86_64-server-release/images/jdk" >> $GITHUB_ENV
```

**Status:** ✅ Correct
- Builds complete JDK images
- Sets JDK_HOME for later steps

### 7. Verify JDK build completeness ✅
```yaml
# Checks:
- java executable
- lib/modules file
- lib/jvm.cfg
- lib/server/libjvm.so
```

**Status:** ✅ Excellent
- Comprehensive validation
- Exits early if incomplete
- Shows file sizes

### 8. Verify JDK runs ✅
```yaml
$JDK_HOME/bin/java -version
```

**Status:** ✅ Good
- Runtime verification
- Ensures JDK actually works

### 9. Check Spark optimization flags ✅
```yaml
$JDK_HOME/bin/java -XX:+UnlockExperimentalVMOptions -XX:+PrintFlagsFinal -version 2>&1 | grep "G1OptimizeForSpark"
```

**Status:** ✅ Excellent
- Verifies custom flags exist
- Confirms JDK has Spark optimizations

### 10. Cache SBT and Ivy ✅
```yaml
path: |
  ~/.sbt
  ~/.ivy2/cache
  ~/.m2/repository
key: ${{ runner.os }}-sbt-${{ hashFiles('**/build.sbt') }}
```

**Status:** ✅ Good
- Caches build dependencies
- Hash-based cache key

### 11. Clone and build Apache Spark ✅
```yaml
git clone --depth 1 --branch java25 https://github.com/wangyum/spark.git spark-src
cd spark-src
./build/sbt clean package Test/packageBin
```

**Status:** ✅ Correct
- Uses custom JDK (JAVA_HOME set)
- Creates test jars with Test/packageBin
- Shallow clone for speed

### 12. Download TPC-DS dataset ✅
```yaml
wget https://github.com/wangyum/tpcds-benchmark/raw/master/tpcds_5GB.tgz
tar -xzf tpcds_5GB.tgz
```

**Status:** ✅ Good
- Pre-generated dataset (fast)
- 5GB scale factor

### 13. Run both benchmarks in parallel ⚠️
```yaml
# BASELINE benchmark in background
./bin/spark-submit \
  --conf spark.driver.extraJavaOptions="-XX:+UseG1GC -Dlog4j.rootCategory=WARN,console" \
  ...
> ../benchmark-baseline.log 2>&1 &

# OPTIMIZED benchmark in background
./bin/spark-submit \
  --conf spark.driver.extraJavaOptions="-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark ..." \
  ...
> ../benchmark-optimized.log 2>&1 &

wait $BASELINE_PID
wait $OPTIMIZED_PID
```

**Status:** ⚠️ Has Issues
- ✅ Parallel execution works well
- ✅ Proper PID tracking
- ❌ **Missing SPARK_CORE_TEST_JAR validation** (same as tpcds-benchmark.yml)
- ❌ **Missing SPARK_SQL_TEST_JAR validation**
- ⚠️ Memory configuration (8GB on 7GB runner)

### 14. Analyze and compare benchmark results ✅
```yaml
python3 .github/scripts/compare_results.py | tee comparison_output.txt
```

**Status:** ✅ Works
- ⚠️ No error handling (script failures ignored)
- ✅ Outputs to both console and file

### 15. Upload benchmark results ✅
```yaml
path: |
  benchmark-baseline.log
  benchmark-optimized.log
  comparison_output.txt
  comparison_results.txt
retention-days: 30
```

**Status:** ✅ Good
- Uploads all relevant files
- 30-day retention

## Critical Issues

### 🚨 Issue 1: Missing Test Jar Validation

**Problem:**
```yaml
# Lines 177-185: Finds test jars
SPARK_CORE_TEST_JAR=$(ls core/target/scala-2.13/spark-core_2.13-*-tests.jar ...)
SPARK_CATALYST_TEST_JAR=$(ls sql/catalyst/target/scala-2.13/spark-catalyst_2.13-*-tests.jar ...)
SPARK_SQL_TEST_JAR=$(ls sql/core/target/scala-2.13/spark-sql_2.13-*-tests.jar ...)

# Lines 188-198: Builds JARS_LIST
# NO VALIDATION - assumes they exist!
```

**Impact:**
- If `SPARK_CORE_TEST_JAR` is missing → NoClassDefFoundError: BenchmarkBase
- If `SPARK_SQL_TEST_JAR` is missing → benchmark won't run
- Benchmarks fail late (after 2+ hours of building)

**Recommendation:**
```bash
# After line 185, add validation:
echo "Test jars found:"
echo "  SPARK_CORE_TEST_JAR=$SPARK_CORE_TEST_JAR"
echo "  SPARK_CATALYST_TEST_JAR=$SPARK_CATALYST_TEST_JAR"
echo "  SPARK_SQL_TEST_JAR=$SPARK_SQL_TEST_JAR"

# Verify required jars exist
if [ -z "$SPARK_CORE_TEST_JAR" ] || [ ! -f "$SPARK_CORE_TEST_JAR" ]; then
  echo "ERROR: spark-core test jar not found!"
  echo "BenchmarkBase class will not be available"
  find core/target -name "*.jar" | head -10
  exit 1
fi

if [ -z "$SPARK_SQL_TEST_JAR" ] || [ ! -f "$SPARK_SQL_TEST_JAR" ]; then
  echo "ERROR: spark-sql test jar not found!"
  find sql/core/target -name "*.jar" | head -10
  exit 1
fi
```

### 🚨 Issue 2: JARS_LIST Construction Doesn't Include SQL Test Jar

**Problem:**
```yaml
# Lines 188-198: Only includes CORE and CATALYST in JARS_LIST
JARS_LIST=""
if [ -n "$SPARK_CORE_TEST_JAR" ] && [ -f "$SPARK_CORE_TEST_JAR" ]; then
  JARS_LIST="$SPARK_CORE_TEST_JAR"
fi
if [ -n "$SPARK_CATALYST_TEST_JAR" ] && [ -f "$SPARK_CATALYST_TEST_JAR" ]; then
  if [ -n "$JARS_LIST" ]; then
    JARS_LIST="$JARS_LIST,$SPARK_CATALYST_TEST_JAR"
  else
    JARS_LIST="$SPARK_CATALYST_TEST_JAR"
  fi
fi

# SQL test jar is passed as main class jar, NOT in --jars!
# This means only CORE and CATALYST are in classpath dependencies
```

**Impact:**
- Currently works because SQL test jar is the main jar
- But JARS_LIST is incomplete conceptually
- Different from tpcds-benchmark.yml approach

**Note:** This actually works correctly, but the pattern is confusing. The SQL test jar contains the main class and is passed as the primary jar argument, while CORE and CATALYST are dependencies.

### ⚠️ Issue 3: Memory Configuration

**Problem:**
```yaml
--driver-memory 4g  # Each benchmark uses 4GB
# 2 benchmarks × 4GB = 8GB total
# GitHub Actions ubuntu-22.04 has ~7GB available
```

**Impact:**
- May cause OOM errors
- Competition for memory between processes

**Recommendation:**
```bash
# Option 1: Reduce to 3GB each (safer)
--driver-memory 3g

# Option 2: Keep 4GB but monitor for OOM
# Current approach is on the edge
```

### ⚠️ Issue 4: No Comparison Script Error Handling

**Problem:**
```yaml
python3 .github/scripts/compare_results.py | tee comparison_output.txt
# Doesn't check exit code
```

**Impact:**
- Workflow succeeds even if comparison fails
- Silent failures

**Recommendation:**
```bash
if ! python3 .github/scripts/compare_results.py | tee comparison_output.txt; then
  echo "WARNING: Comparison script failed"
  echo "Continuing to upload partial results..."
fi
```

## Resource Usage

### Disk Space
- JDK build: ~2 GB
- Spark build: ~2 GB
- TPC-DS dataset: ~800 MB
- Test jars: ~50 MB
- Logs: ~10 MB
- **Total: ~5 GB** (well within 14 GB limit) ✅

### Memory
- JDK build: ~2 GB peak
- Spark build: ~2 GB peak
- Benchmark baseline: 4 GB
- Benchmark optimized: 4 GB
- **Peak: 8 GB** (2 benchmarks running) ⚠️
- Available: ~7 GB on ubuntu-22.04

### CPU
- Both benchmarks: local[*] (all cores)
- **Contention expected** but fair for comparison ⚠️

### Runtime
- JDK install deps: ~2 min
- Bootstrap JDK: ~1 min
- JDK build: ~90 min
- Spark build: ~30 min
- Dataset download: ~2 min
- Benchmarks (parallel): ~40 min
- Comparison: ~1 min
- **Total: ~165 min** (2h 45min) ⚠️
- Note: Workflow timeout is 720 min (12 hours) ✅

## Comparison with tpcds-benchmark.yml

| Feature | spark-benchmark.yml | tpcds-benchmark.yml |
|---------|-------------------|-------------------|
| **Purpose** | Custom JDK opts comparison | GC comparison |
| **JDK** | Custom built JDK 25 | Standard JDK 25 |
| **Build Time** | +90 min (JDK build) | No JDK build |
| **Comparison** | Baseline vs Optimized | G1GC vs ZGC |
| **Total Time** | ~165 min | ~73 min |
| **Complexity** | Higher | Lower |
| **Test Jar Validation** | ❌ Missing | ⚠️ Partial (missing core) |
| **Memory Config** | ⚠️ 8GB on 7GB runner | ⚠️ 8GB on 7GB runner |
| **Error Handling** | ⚠️ Partial | ⚠️ Partial |

## Strengths

### 1. Comprehensive JDK Validation ✅
- Checks critical files (modules, jvm.cfg, libjvm.so)
- Verifies JDK runs
- Confirms custom flags present
- Best practice validation

### 2. Efficient Caching ✅
- SBT and Ivy dependencies cached
- Hash-based cache keys
- Speeds up subsequent runs

### 3. Parallel Execution ✅
- Both benchmarks run simultaneously
- Saves ~40 minutes vs sequential
- Proper PID tracking

### 4. Clean Logging ✅
- WARN level reduces clutter
- Separate logs per benchmark
- Easy to analyze

### 5. Path Filters ✅
```yaml
paths:
  - 'src/**'
  - '.github/workflows/spark-benchmark.yml'
```
- Only runs when relevant files change
- Saves runner time

## Weaknesses

### 1. ❌ Missing Test Jar Validation
- No check for SPARK_CORE_TEST_JAR existence
- No check for SPARK_SQL_TEST_JAR existence
- Fails late after 2+ hours

### 2. ⚠️ Memory Configuration
- 8GB requested on ~7GB runner
- Risk of OOM

### 3. ⚠️ CPU Contention
- Both use local[*]
- Fair comparison but may slow both down

### 4. ⚠️ Long Runtime
- 165 minutes total
- Most of time spent building custom JDK
- Can't be parallelized (single job design)

### 5. ℹ️ No Error Handling for Comparison
- Script failures ignored
- Silent partial results

## Recommendations

### Priority 1: Add Test Jar Validation ⚠️ CRITICAL

**Add after line 185:**
```bash
echo "Test jars found:"
echo "  SPARK_CORE_TEST_JAR=$SPARK_CORE_TEST_JAR"
echo "  SPARK_CATALYST_TEST_JAR=$SPARK_CATALYST_TEST_JAR"
echo "  SPARK_SQL_TEST_JAR=$SPARK_SQL_TEST_JAR"

# Verify CORE test jar (required for BenchmarkBase)
if [ -z "$SPARK_CORE_TEST_JAR" ] || [ ! -f "$SPARK_CORE_TEST_JAR" ]; then
  echo "ERROR: spark-core test jar not found!"
  echo "BenchmarkBase class will not be available"
  find core/target -name "*.jar" | head -10
  exit 1
fi

# Verify SQL test jar (required for TPCDSQueryBenchmark)
if [ -z "$SPARK_SQL_TEST_JAR" ] || [ ! -f "$SPARK_SQL_TEST_JAR" ]; then
  echo "ERROR: spark-sql test jar not found!"
  find sql/core/target -name "*.jar" | head -10
  exit 1
fi
```

**Why Critical:**
- Prevents 2+ hour wasted build only to fail at benchmark step
- Provides clear error message
- Shows what jars are actually present

### Priority 2: Consider Memory Reduction

**Change lines 211, 233:**
```yaml
# From:
--driver-memory 4g

# To:
--driver-memory 3g
```

**Why Important:**
- 6GB total (3GB × 2) fits comfortably in 7GB available
- Reduces risk of OOM
- Still sufficient for 5GB dataset

### Priority 3: Add Comparison Error Handling

**Change line 292:**
```yaml
# From:
python3 .github/scripts/compare_results.py | tee comparison_output.txt

# To:
if ! python3 .github/scripts/compare_results.py | tee comparison_output.txt; then
  echo "WARNING: Comparison script failed"
  echo "Continuing to upload partial results..."
fi
```

**Why Important:**
- Catches script failures
- Provides visibility
- Still uploads logs for debugging

### Optional: Add Progress Monitoring

**Add after line 250:**
```yaml
echo ""
echo "Waiting for benchmarks to complete..."
echo "This will take approximately 40 minutes"
echo "BASELINE: benchmark-baseline.log"
echo "OPTIMIZED: benchmark-optimized.log"
echo ""
```

**Why Useful:**
- Sets expectations
- Shows where to find logs
- Improves user experience

## Benchmark Configuration

### Baseline:
```yaml
--conf spark.driver.extraJavaOptions="-XX:+UseG1GC -Dlog4j.rootCategory=WARN,console"
```
- Standard G1GC
- No custom optimizations

### Optimized:
```yaml
--conf spark.driver.extraJavaOptions="-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization -Dlog4j.rootCategory=WARN,console"
```
- Custom flags enabled:
  - `G1OptimizeForSpark` - Master flag
  - `G1SparkEnhanceEscapeAnalysis` - Escape analysis improvements
  - `G1SparkOptimizeHashOperations` - Hash operation optimizations
  - `G1SparkEnableVectorization` - Vectorization support

## Query Filter

Both benchmarks run same queries:
```
q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96
```

14 queries selected from TPC-DS suite (total 99 queries available)

## Workflow Dispatch Options

```yaml
workflow_dispatch:
  inputs:
    benchmark_scale:
      description: 'Benchmark scale factor (small/medium/large)'
      default: 'small'
      type: choice
```

**Note:** This input is defined but **NOT USED** in the workflow!
- No conditional logic based on scale
- Always uses same dataset and queries
- Should either implement scale logic or remove input

## Summary

**Overall Grade: B+** (Good but needs fixes)

**Strengths:**
- ✅ Comprehensive JDK build validation
- ✅ Efficient caching and parallel execution
- ✅ Clean logging and output
- ✅ Proper custom JDK with Spark optimizations
- ✅ Good path filters

**Critical Issues:**
- ❌ Missing test jar validation (will fail after 2+ hours)
- ⚠️ Memory configuration risky (8GB on 7GB runner)
- ⚠️ No comparison script error handling
- ℹ️ Unused workflow_dispatch input

**Recommendations:**
1. **MUST FIX:** Add test jar validation (Priority 1)
2. **SHOULD FIX:** Reduce memory to 3GB per benchmark (Priority 2)
3. **SHOULD FIX:** Add comparison error handling (Priority 3)
4. **CONSIDER:** Remove or implement benchmark_scale input

**Verdict:** Production-usable but **strongly recommend** implementing Priority 1 fix to prevent wasted builds.

**Time Investment:**
- ~165 minutes per run
- Expensive to fail late
- Validation is critical

**File:** `.github/workflows/spark-benchmark.yml` (305 lines)
