# Separate Spark Directories for True Parallel Execution

## Problem

Even with pre-compilation, running two SBT commands from the same directory causes interference:

```bash
cd spark-src
./build/sbt "sql/Test/runMain ..." &  # Process 1
./build/sbt "sql/Test/runMain ..." &  # Process 2
```

**Issues:**
- ❌ Both processes use the **same SBT server/daemon**
- ❌ File locking conflicts in `target/` directories
- ❌ One process may wait for the other
- ❌ Not truly parallel execution
- ❌ SBT state synchronization overhead

## Solution: Separate Spark Directories

Copy Spark to two independent directories:

```bash
spark-baseline/     # For baseline benchmark
spark-optimized/    # For optimized benchmark
```

Each runs SBT independently with **zero interference**!

## Implementation

### Step 1: Build Spark Once
```bash
git clone https://github.com/wangyum/spark.git spark-src
cd spark-src
./build/sbt package  # Build once
```

### Step 2: Create Two Copies
```bash
cp -r spark-src spark-baseline
cp -r spark-src spark-optimized
```

### Step 3: Run in Parallel from Different Directories
```bash
# BASELINE in spark-baseline/
(
  cd spark-baseline
  export JAVA_OPTS="-XX:+UseG1GC"
  ./build/sbt "sql/Test/runMain Benchmark ..."
) &

# OPTIMIZED in spark-optimized/
(
  cd spark-optimized
  export JAVA_OPTS="-XX:+UseG1GC -XX:+G1OptimizeForSpark ..."
  ./build/sbt "sql/Test/runMain Benchmark ..."
) &

wait  # Both run independently!
```

## Benefits

### 1. True Parallel Execution ✅
- Each SBT instance has its own server
- No file locking conflicts
- No waiting for each other
- Full CPU utilization

### 2. No SBT Interference ✅
- Separate `target/` directories
- Separate SBT state
- Separate incremental compilation state
- Separate JVM processes

### 3. Fair Comparison ✅
- Both start nearly simultaneously
- Both compile from same source
- Only difference is JVM flags
- No cross-contamination

### 4. Clean Logs ✅
- Each writes to its own log file
- No mixed output
- Clear separation of results

### 5. Disk Space Trade-off ⚖️
- Uses more disk space (~2x Spark source)
- But GitHub Actions has plenty of space
- Worth it for true parallel execution

## Workflow Changes

### Before (Shared Directory)
```yaml
- name: Clone and build Spark
  run: |
    git clone ... spark-src
    cd spark-src
    ./build/sbt package
    echo "SPARK_HOME=$(pwd)" >> $GITHUB_ENV

- name: Run benchmarks
  run: |
    cd $SPARK_HOME
    ./build/sbt "runMain ..." &  # Both from same dir
    ./build/sbt "runMain ..." &
    wait
```

**Problems:**
- SBT server conflicts
- File locking issues
- Not truly parallel

### After (Separate Directories)
```yaml
- name: Clone and build Spark
  run: |
    git clone ... spark-src
    cd spark-src
    ./build/sbt package

- name: Create separate copies
  run: |
    cp -r spark-src spark-baseline
    cp -r spark-src spark-optimized

- name: Run benchmarks in parallel
  run: |
    (cd spark-baseline && ./build/sbt "runMain ...") &
    (cd spark-optimized && ./build/sbt "runMain ...") &
    wait
```

**Benefits:**
- Independent SBT servers
- No file conflicts
- Truly parallel execution

## Directory Structure

```
workspace/
├── spark-src/          # Original build (can be deleted after copy)
├── spark-baseline/     # Copy 1 - for baseline benchmark
│   ├── build/
│   ├── target/         # Independent target directory
│   └── ...
├── spark-optimized/    # Copy 2 - for optimized benchmark
│   ├── build/
│   ├── target/         # Independent target directory
│   └── ...
├── tpcds_5GB/          # Shared dataset (read-only)
├── benchmark-baseline.log
└── benchmark-optimized.log
```

## Disk Usage

**Before (shared):**
- `spark-src/`: ~2 GB (source + build artifacts)
- **Total:** ~2 GB

**After (separate):**
- `spark-baseline/`: ~2 GB
- `spark-optimized/`: ~2 GB
- **Total:** ~4 GB

**GitHub Actions runner:**
- Available disk: ~14 GB SSD
- Usage: 4 GB / 14 GB = **29%**
- ✅ Plenty of space available!

## Performance Impact

### Build Time
- **Before:** 30 min (build once)
- **After:** 30 min (build once) + 10 sec (copy)
- **Overhead:** ~10 seconds (negligible)

### Copy Time
```bash
time cp -r spark-src spark-baseline
# Real: ~5 seconds (SSD)

time cp -r spark-src spark-optimized
# Real: ~5 seconds (SSD)
```

**Total copy overhead:** ~10 seconds

### Benchmark Execution
- **Before:** Variable (SBT conflicts cause slowdowns)
- **After:** Consistent (no conflicts)
- **Improvement:** More predictable, potentially faster

## Why cp -r Works Well

1. **All compiled artifacts included:**
   - `target/` directories with compiled classes
   - `build/` directory with jars
   - SBT cache and state

2. **Fast on SSD:**
   - GitHub Actions runners use SSD
   - ~2 GB copies in ~5 seconds

3. **Preserves everything:**
   - File permissions
   - Timestamps
   - Symbolic links

4. **Simple and reliable:**
   - No complex scripting needed
   - Works every time

## Alternative Approaches Considered

### ❌ Approach 1: Pre-compile then share
```bash
./build/sbt compile
./build/sbt runMain ... &  # Both from same dir
./build/sbt runMain ... &
```
**Problem:** Still share SBT server and file state

### ❌ Approach 2: Manual classpath
```bash
./build/sbt exportClasspath
java -cp $CLASSPATH ... &
java -cp $CLASSPATH ... &
```
**Problem:** `exportClasspath` doesn't exist, complex to build manually

### ✅ Approach 3: Separate directories (CHOSEN)
```bash
cp -r spark-src spark-baseline
cp -r spark-src spark-optimized
(cd spark-baseline && sbt ...) &
(cd spark-optimized && sbt ...) &
```
**Benefits:** Simple, reliable, truly independent

## Edge Cases Handled

### Shared Data (Dataset)
- TPC-DS dataset is **read-only**
- Both benchmarks can safely share it
- No need to copy dataset

### JDK
- Both use same JDK (`$JAVA_HOME`)
- Only JVM flags differ
- This is correct for fair comparison

### Logs
- Written to `$GITHUB_WORKSPACE/` (parent directory)
- Not in Spark directories
- No conflicts

## Verification

To verify both run independently:

```bash
# Check different PIDs
ps aux | grep "sbt.*Benchmark" | grep baseline
ps aux | grep "sbt.*Benchmark" | grep optimized

# Check different working directories
lsof -p $BASELINE_PID | grep cwd
lsof -p $OPTIMIZED_PID | grep cwd

# Should show:
# baseline: /workspace/spark-baseline
# optimized: /workspace/spark-optimized
```

## Summary

**Problem:** SBT interference when running from same directory
**Solution:** Copy Spark to two directories, run independently
**Cost:** ~4 GB disk (vs 2 GB), ~10 sec copy time
**Benefit:** True parallel execution, no interference, fair comparison

This is the **simplest and most reliable** way to achieve truly parallel benchmark execution with SBT!
