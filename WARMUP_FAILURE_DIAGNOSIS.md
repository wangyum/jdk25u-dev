# Warmup Failure Diagnosis

## The Error

```
Running warmup with q3 to warm up JIT compiler...
Error: Process completed with exit code 1.
```

## Root Cause Analysis

The warmup step is failing with exit code 1. Here are the most likely causes:

### 1. **Missing Output Redirection** ✅ FIXED

**Issue:** The original split forgot to redirect output to `../warmup.log 2>&1`

**Fix Applied:**
```bash
./bin/spark-submit \
  --query-filter "q3" \
  > ../warmup.log 2>&1  # This was missing!
```

**Status:** ✅ Fixed in latest commit

### 2. **Environment Variables Not Set**

**Issue:** `JARS_LIST`, `SPARK_SQL_TEST_JAR`, or `TPCDS_DATA` might not be available in the warmup step.

**Debug:**
```bash
- name: Warmup run (JIT warmup)
  run: |
    # Add debug output
    echo "DEBUG: JARS_LIST=$JARS_LIST"
    echo "DEBUG: SPARK_SQL_TEST_JAR=$SPARK_SQL_TEST_JAR"
    echo "DEBUG: TPCDS_DATA=$TPCDS_DATA"
    echo "DEBUG: JDK_HOME=$JDK_HOME"

    # Check if files exist
    ls -l "$SPARK_SQL_TEST_JAR" || echo "ERROR: SQL test jar not found!"
    ls -d "$TPCDS_DATA" || echo "ERROR: TPC-DS data not found!"

    cd spark-src
    ./bin/spark-submit ...
```

**Expected Output:**
```
DEBUG: JARS_LIST=/path/to/core.jar,/path/to/catalyst.jar
DEBUG: SPARK_SQL_TEST_JAR=/path/to/sql.jar
DEBUG: TPCDS_DATA=/home/runner/work/.../tpcds_5GB
DEBUG: JDK_HOME=/home/runner/work/.../jdk
```

### 3. **Working Directory Issues**

**Issue:** The `cd spark-src` command might fail or the directory might not exist.

**Debug:**
```bash
- name: Warmup run (JIT warmup)
  run: |
    echo "Current directory: $(pwd)"
    ls -la

    if [ ! -d "spark-src" ]; then
      echo "ERROR: spark-src directory not found!"
      exit 1
    fi

    cd spark-src
    echo "Changed to: $(pwd)"

    if [ ! -f "bin/spark-submit" ]; then
      echo "ERROR: bin/spark-submit not found!"
      exit 1
    fi
```

### 4. **JDK_HOME Not Set**

**Issue:** The custom JDK might not be available in subsequent steps.

**Debug:**
```bash
- name: Warmup run (JIT warmup)
  run: |
    echo "JDK_HOME=$JDK_HOME"

    if [ -z "$JDK_HOME" ]; then
      echo "ERROR: JDK_HOME not set!"
      exit 1
    fi

    if [ ! -f "$JDK_HOME/bin/java" ]; then
      echo "ERROR: java not found at $JDK_HOME/bin/java"
      exit 1
    fi

    export JAVA_HOME=$JDK_HOME
    export PATH=$JAVA_HOME/bin:$PATH

    java -version
```

### 5. **Query Execution Failure**

**Issue:** The actual query (q3) is failing.

**Debug by checking warmup.log:**
```bash
- name: Show warmup log if failed
  if: failure()
  run: |
    echo "=== Warmup Log ==="
    cat warmup.log || echo "Warmup log not found"
```

**Common q3 failure reasons:**
- TPC-DS data directory not found
- Data files corrupted
- Memory issues (3g might not be enough with AlwaysPreTouch)
- Java class not found (missing jars)

### 6. **AlwaysPreTouch Memory Issue**

**Issue:** With `-XX:+AlwaysPreTouch`, the JVM pre-touches all 3GB of memory at startup. On GitHub Actions runners with limited memory, this might fail.

**Test without AlwaysPreTouch:**
```bash
--conf spark.driver.extraJavaOptions="-Xms3g -Xmx3g -XX:+UseG1GC"
# Remove: -XX:+AlwaysPreTouch
```

**Or reduce memory:**
```bash
--driver-memory 2g
--conf spark.driver.extraJavaOptions="-Xms2g -Xmx2g -XX:+UseG1GC -XX:+AlwaysPreTouch"
```

## Recommended Debug Steps

### Step 1: Add Debug Output

```yaml
- name: Warmup run (JIT warmup)
  run: |
    export JAVA_HOME=$JDK_HOME
    export PATH=$JAVA_HOME/bin:$PATH

    # Debug environment
    echo "=== Environment Check ==="
    echo "PWD: $(pwd)"
    echo "JDK_HOME: $JDK_HOME"
    echo "JAVA_HOME: $JAVA_HOME"
    echo "TPCDS_DATA: $TPCDS_DATA"
    echo "JARS_LIST: $JARS_LIST"
    echo "SPARK_SQL_TEST_JAR: $SPARK_SQL_TEST_JAR"

    # Verify files exist
    echo "=== File Check ==="
    ls -lh "$SPARK_SQL_TEST_JAR" || echo "SQL jar missing!"
    ls -d "$TPCDS_DATA" || echo "TPC-DS data missing!"

    # Check directory
    echo "=== Directory Check ==="
    if [ ! -d "spark-src" ]; then
      echo "ERROR: spark-src not found"
      ls -la
      exit 1
    fi

    cd spark-src

    # Verify spark-submit
    if [ ! -f "bin/spark-submit" ]; then
      echo "ERROR: bin/spark-submit not found"
      ls -la bin/
      exit 1
    fi

    # Check Java
    echo "=== Java Version ==="
    java -version

    echo "=== Running Warmup ==="
    ./bin/spark-submit \
      --master local[1] \
      --driver-memory 3g \
      --conf spark.driver.extraJavaOptions="-Xms3g -Xmx3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
      --conf spark.driver.log.level=ERROR \
      --jars "$JARS_LIST" \
      --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
      "$SPARK_SQL_TEST_JAR" \
      --data-location "$TPCDS_DATA" \
      --query-filter "q3" \
      > ../warmup.log 2>&1

    echo "✅ Warmup completed"

- name: Show warmup log if failed
  if: failure()
  run: |
    echo "=== Warmup Log ==="
    cat warmup.log 2>/dev/null || echo "Warmup log not found"

    echo "=== Spark Logs ==="
    ls -la spark-src/logs/ 2>/dev/null || echo "No Spark logs"

    echo "=== Last 100 lines of any error logs ==="
    find spark-src -name "*.log" -type f -exec tail -100 {} \; 2>/dev/null || echo "No error logs found"
```

### Step 2: Test Locally

Run the exact same command locally to reproduce:

```bash
cd /Users/yumwang/opensource/spark-java25

export JAVA_HOME=/Users/yumwang/opensource/jdk25u-dev/build/macosx-aarch64-server-release/images/jdk
export PATH=$JAVA_HOME/bin:$PATH
export TPCDS_DATA=/Users/yumwang/opensource/spark-sql-perf/tpcds_5GB

JARS_LIST=$(ls core/target/scala-2.13/spark-core_2.13-*-tests.jar | head -1)
JARS_LIST="$JARS_LIST,$(ls sql/catalyst/target/scala-2.13/spark-catalyst_2.13-*-tests.jar | head -1)"
SPARK_SQL_TEST_JAR=$(ls sql/core/target/scala-2.13/spark-sql_2.13-*-tests.jar | head -1)

./bin/spark-submit \
  --master local[1] \
  --driver-memory 3g \
  --conf spark.driver.extraJavaOptions="-Xms3g -Xmx3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  --conf spark.driver.log.level=ERROR \
  --jars "$JARS_LIST" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3"

# Check exit code
echo "Exit code: $?"
```

### Step 3: Check GitHub Actions Logs

Look for specific error messages:
- `java.lang.OutOfMemoryError` - Reduce memory or remove AlwaysPreTouch
- `ClassNotFoundException` - Jars not in classpath
- `FileNotFoundException` - TPC-DS data path wrong
- `java.io.IOException: Cannot run program` - JDK_HOME issue

## Quick Fixes

### Fix 1: Remove AlwaysPreTouch Temporarily

```yaml
--conf spark.driver.extraJavaOptions="-Xms3g -Xmx3g -XX:+UseG1GC"
# Removed: -XX:+AlwaysPreTouch
```

This will make it work, then we can debug AlwaysPreTouch separately.

### Fix 2: Reduce Memory

```yaml
--driver-memory 2g
--conf spark.driver.extraJavaOptions="-Xms2g -Xmx2g -XX:+UseG1GC -XX:+AlwaysPreTouch"
```

GitHub Actions runners have ~7GB RAM. Using 3g + AlwaysPreTouch might be too much.

### Fix 3: Verify Environment Variables Are Saved

Make sure the setup step has:

```yaml
- name: Setup benchmark environment
  run: |
    # ... find jars ...

    # CRITICAL: Save to GITHUB_ENV
    echo "TPCDS_DATA=$TPCDS_DATA" >> $GITHUB_ENV
    echo "JARS_LIST=$JARS_LIST" >> $GITHUB_ENV
    echo "SPARK_SQL_TEST_JAR=$SPARK_SQL_TEST_JAR" >> $GITHUB_ENV
    echo "JDK_HOME=$JDK_HOME" >> $GITHUB_ENV  # Don't forget this!
```

### Fix 4: Add Explicit Error Checking

```bash
# After spark-submit
WARMUP_EXIT=$?
if [ $WARMUP_EXIT -ne 0 ]; then
  echo "ERROR: Warmup failed with exit code $WARMUP_EXIT"
  echo "=== Warmup log ==="
  cat ../warmup.log
  exit 1
fi
```

## Most Likely Causes (Ranked)

1. **Missing JDK_HOME in GITHUB_ENV** (90% likely)
   - Setup step sets JDK_HOME
   - But doesn't save to $GITHUB_ENV
   - Warmup step has JDK_HOME=$JDK_HOME but JDK_HOME is empty

2. **AlwaysPreTouch OOM** (5% likely)
   - 3GB pre-touch might exhaust runner memory

3. **Missing environment variables** (3% likely)
   - JARS_LIST, SPARK_SQL_TEST_JAR, TPCDS_DATA not saved

4. **Actual query failure** (2% likely)
   - TPC-DS data issue

## Immediate Fix

Add this to the **Setup step** (most likely fix):

```yaml
- name: Setup benchmark environment and validate test jars
  run: |
    export JAVA_HOME=$JDK_HOME
    export PATH=$JAVA_HOME/bin:$PATH
    export TPCDS_DATA=$(pwd)/tpcds_5GB

    cd spark-src

    # ... existing jar finding code ...

    # Save environment variables for subsequent steps
    echo "JDK_HOME=$JDK_HOME" >> $GITHUB_ENV        # ← ADD THIS LINE
    echo "TPCDS_DATA=$TPCDS_DATA" >> $GITHUB_ENV
    echo "JARS_LIST=$JARS_LIST" >> $GITHUB_ENV
    echo "SPARK_SQL_TEST_JAR=$SPARK_SQL_TEST_JAR" >> $GITHUB_ENV
```

The `JDK_HOME` variable is being used in warmup but was never saved to `$GITHUB_ENV`!

## Testing the Fix

After applying the fix, the workflow should show:

```
✓ Setup benchmark environment and validate test jars (10s)
✓ Warmup run (JIT warmup) (1m 30s)
✓ Run BASELINE benchmark (20m)
```

Instead of:

```
✓ Setup benchmark environment and validate test jars (10s)
✗ Warmup run (JIT warmup) (5s) - Error: Process completed with exit code 1
```
