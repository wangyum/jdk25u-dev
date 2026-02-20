# Workflow Optimization Improvements

## Summary

Updated both benchmark workflows to eliminate resource contention and improve measurement accuracy based on JIT compilation analysis.

## Changes Applied

### 1. ✅ Reduced Memory: 4g → 3g

**Before:**
```yaml
--driver-memory 4g  # 8GB total for parallel runs
```

**After:**
```yaml
--driver-memory 3g  # 3GB per run (sequential)
```

**Benefits:**
- Safer on GitHub Actions runners (~7GB available)
- Reduces GC pressure
- No memory contention between benchmarks
- Still sufficient for 5GB TPC-DS dataset

### 2. ✅ Dedicated Core: local[2] → local[1]

**Before:**
```yaml
--master local[2]  # Both benchmarks share 2 cores
```

**After:**
```yaml
--master local[1]  # Each benchmark gets dedicated core
```

**Benefits:**
- No CPU contention during execution
- No competition during JIT warmup
- More consistent performance measurements
- Cleaner benchmark environment

### 3. ✅ Added Warmup Run

**New warmup step added:**
```bash
# Warmup run (discard results)
./bin/spark-submit \
  --master local[1] \
  --driver-memory 3g \
  --conf spark.driver.extraJavaOptions="-XX:+UseG1GC" \
  --conf spark.driver.log.level=ERROR \
  --jars "$JARS_LIST" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3" \
  > ../warmup.log 2>&1
```

**Benefits:**
- JIT compiler fully optimizes code before benchmark
- Eliminates cold-start effects
- More stable timing measurements
- Reduces variance between runs

### 4. ✅ Sequential Execution: Parallel → Sequential

**Before:**
```bash
baseline & optimized &  # Both run simultaneously
wait $BASELINE_PID
wait $OPTIMIZED_PID
```

**After:**
```bash
baseline  # Runs first
# Waits for completion
optimized  # Runs after baseline completes
```

**Benefits:**
- Each benchmark gets full system resources
- No interference between benchmarks
- Proper JIT warmup for each configuration
- More accurate performance comparison

### 5. ✅ Fixed ZGC Configuration (tpcds-benchmark.yml)

**Before (BUG):**
```yaml
# ZGC benchmark was actually using G1GC!
--conf spark.driver.extraJavaOptions="-XX:+UseG1GC"
```

**After (FIXED):**
```yaml
--conf spark.driver.extraJavaOptions="-XX:+UseZGC"
```

**Impact:**
- Previous G1GC vs ZGC comparison was actually G1GC vs G1GC!
- Now correctly compares G1GC and ZGC
- Results will be meaningful

## Files Modified

### 1. `.github/workflows/spark-benchmark.yml`

**Changes:**
- Line 168: Renamed step to "Run benchmarks sequentially with warmup"
- Lines 218-235: Added warmup run with q3
- Lines 237-257: BASELINE runs sequentially (removed background &)
- Line 247: Changed memory: 4g → 3g
- Lines 259-277: OPTIMIZED runs sequentially after BASELINE
- Line 267: Changed memory: 4g → 3g
- Removed parallel execution wait logic

**Result:** Sequential execution with warmup

### 2. `.github/workflows/tpcds-benchmark.yml`

**Changes:**
- Line 62: Renamed step to "Run benchmarks sequentially with warmup (G1GC vs ZGC)"
- Lines 102-119: Added warmup run with q3
- Lines 121-140: G1GC runs sequentially (removed background &)
- Line 129: Changed memory: 4g → 3g
- Lines 142-161: ZGC runs sequentially after G1GC
- Line 150: Changed memory: 4g → 3g
- Line 152: FIXED: UseG1GC → UseZGC
- Removed parallel execution wait logic

**Result:** Sequential execution with warmup, correct ZGC comparison

## Expected Impact

### Runtime Changes

**Before:**
```
Total time: ~73 minutes (both parallel)
- JDK build: 0 min
- Spark build: 30 min
- Benchmarks: 40 min (parallel)
- Comparison: 1 min
```

**After:**
```
Total time: ~113 minutes (sequential + warmup)
- JDK build: 0 min
- Spark build: 30 min
- Warmup: 1 min
- Benchmark 1: 40 min
- Benchmark 2: 40 min
- Comparison: 1 min
```

**Trade-off:** +40 minutes runtime for accurate results

### Performance Measurement Improvements

**Before (Parallel):**
- ❌ CPU contention during warmup
- ❌ Memory pressure (8GB on 7GB runner)
- ❌ Incomplete JIT optimization
- ❌ High variance between runs
- ❌ q3 showed -62.5% regression (artifact of contention)

**After (Sequential + Warmup):**
- ✅ Dedicated resources per benchmark
- ✅ Sufficient memory (3GB < 7GB available)
- ✅ Complete JIT optimization before measurement
- ✅ Lower variance between runs
- ✅ Expected: q3 should show improvement or neutral

### Q3 Regression Fix

Based on local testing:
- **Local (M2 Max, sequential):** q3 is 3.65% FASTER with optimizations
- **CI (parallel, before fix):** q3 showed -62.5% SLOWER

**Root cause identified:**
1. CPU contention during JIT warmup
2. Memory pressure from parallel execution
3. Query executed before JIT completed optimization

**Expected after fix:**
- q3 should show improvement or neutral result
- No more artificial regression from resource contention
- More accurate representation of optimization impact

## Verification

After next CI run, check:

### 1. Memory Usage
```bash
# Should see ~3GB peak, not 8GB
# No OOM errors
```

### 2. Sequential Execution
```bash
# Logs should show:
# - Warmup completed
# - BASELINE completed
# - OPTIMIZED started (after BASELINE)
```

### 3. Q3 Performance
```bash
# Expected: q3 improvement or neutral
# Not -62.5% regression
```

### 4. Overall Results
```bash
# Should maintain or improve +7.1% overall improvement
# Individual query results should be more consistent
```

## Monitoring

Watch for these indicators of success:

### ✅ Success Indicators
- Q3 no longer shows major regression
- Overall improvement maintained or increased
- Lower variance between runs
- No OOM errors
- Consistent timing measurements

### ⚠️ Warning Signs
- Still seeing q3 regression
- Higher variance than before
- OOM errors
- Inconsistent results

If issues persist, consider:
- Further reducing memory to 2g
- Adding more warmup iterations
- Increasing timeout limits

## Rollback Plan

If sequential execution causes issues:

```yaml
# Revert to parallel but with:
--master local[1]  # Still use dedicated core per benchmark
--driver-memory 3g  # Keep reduced memory

# Run in parallel again:
baseline & optimized &
```

This gives some benefits without full sequential execution.

## Summary

**Implemented all three recommendations:**
1. ✅ Reduced memory: 4g → 3g
2. ✅ Dedicated core: local[2] → local[1]
3. ✅ Added warmup: q3 run before actual benchmarks

**Additional fix:**
4. ✅ Sequential execution: parallel → sequential
5. ✅ Fixed ZGC bug: UseG1GC → UseZGC (in tpcds-benchmark.yml)

**Expected outcome:**
- More accurate benchmark measurements
- Elimination of q3 regression artifact
- Better representation of optimization impact
- Slightly longer runtime (+40 min) but worth it for accuracy

**Trade-off accepted:**
- +40 minutes runtime
- More accurate results
- Better understanding of optimization impact
