# Spark Optimization Flags Disabled (Performance Regression)

## Status: ⚠️ DISABLED BY DEFAULT

Spark optimization flags have been **disabled by default** in `spark-benchmark.yml` due to performance regression.

## Regression Data

**Overall Performance:** -2.5% slower with optimizations
- Baseline: 14,928ms
- Optimized: 15,305ms

**Query-by-Query Results:**

| Query | Baseline (ms) | Optimized (ms) | Change |
|-------|---------------|----------------|---------|
| q3    | 426           | 613            | **-43.9%** ⚠️ |
| q27   | 1028          | 1097           | -6.7% |
| q73   | 563           | 608            | -8.0% |
| q79   | 667           | 719            | -7.8% |
| q43   | 1086          | 1023           | +5.8% ✅ |
| q19   | 450           | 436            | +3.1% ✅ |
| q55   | 134           | 130            | +3.0% ✅ |

## Flags Affected

These flags are now disabled in the default configuration:

```bash
-XX:+G1OptimizeForSpark
-XX:+G1SparkEnhanceEscapeAnalysis
-XX:+G1SparkOptimizeHashOperations
-XX:+G1SparkEnableVectorization
```

## New Default Configuration

### Before (Caused Regression):
```yaml
optimized_opts: '-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization'
```

### After (Safe):
```yaml
optimized_opts: '-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch'
```

Both baseline and optimized now use the same configuration until flags are fixed.

## Investigation Tools Created

### 1. Bisection Test Script
```bash
./bisect-spark-optimizations.sh
```

Runs 6 tests to identify which flag causes the regression:
- Test 0: Baseline (no optimizations)
- Test 1: Only G1OptimizeForSpark
- Test 2: G1OptimizeForSpark + EscapeAnalysis
- Test 3: G1OptimizeForSpark + HashOperations
- Test 4: G1OptimizeForSpark + Vectorization
- Test 5: All optimizations (confirms regression)

### 2. Diagnostic Test Script
```bash
./test-with-diagnostics.sh [query]
```

Runs benchmark with full JVM diagnostics enabled:
- GC logging
- JIT compilation logging
- Deoptimization tracking
- Performance counters

### 3. Single Flag Test Script
```bash
./test-single-flag.sh <flag-name> [query]
```

Tests a specific flag in isolation:
```bash
./test-single-flag.sh G1SparkEnhanceEscapeAnalysis q3
./test-single-flag.sh G1SparkOptimizeHashOperations q3
```

## Manual Testing

You can still enable the flags manually for testing:

```bash
# Test all optimizations
gh workflow run spark-benchmark.yml --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization" \
  -f query_filter="q3"

# Test single flag
gh workflow run spark-benchmark.yml --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis" \
  -f query_filter="q3"
```

## Next Steps

1. **Run bisection tests** to identify problematic flag(s)
   ```bash
   ./bisect-spark-optimizations.sh
   ```

2. **Analyze results** from bisection to find the culprit

3. **Run diagnostics** on the problematic flag
   ```bash
   ./test-with-diagnostics.sh q3
   ```

4. **Review implementation** of the problematic optimization in JDK source

5. **Fix the bug** or **tune parameters**

6. **Re-enable** flags after validation

## Suspected Root Causes

Based on code review, potential issues:

### G1SparkEnhanceEscapeAnalysis
- May be too aggressive in escape analysis
- Could cause deoptimization
- Scalar replacement might fail, wasting analysis time

### G1SparkOptimizeHashOperations
- Assembly-optimized MurmurHash3
- Platform-specific issues possible
- Might not be faster on all CPUs

### G1SparkEnableVectorization
- SIMD vectorization attempts
- May fail and fall back with overhead
- Alignment issues possible

### G1OptimizeForSpark (G1GC Tuning)
- Changes multiple G1GC parameters
- Tuning might not match TPC-DS workload
- String dedup overhead may exceed benefits

## Files Modified

- `.github/workflows/spark-benchmark.yml` - Disabled optimizations in defaults
- `bisect-spark-optimizations.sh` - Created bisection test script
- `test-with-diagnostics.sh` - Created diagnostic test script
- `test-single-flag.sh` - Created single flag test script
- `SPARK_OPT_REGRESSION_ANALYSIS.md` - Detailed analysis
- `SPARK_OPT_INVESTIGATION_PLAN.md` - Investigation plan
- `OPTIMIZATION_FLAGS_DISABLED.md` - This document

## Impact

**Automatic runs (on push):** Now use safe configuration (no optimizations)

**Manual runs:** Can still enable optimizations via workflow_dispatch inputs

**Testing:** Use provided scripts to investigate and fix the regression

## When Will Flags Be Re-enabled?

Flags will be re-enabled when:

1. ✅ Root cause identified through bisection
2. ✅ Bug fixed or parameters tuned
3. ✅ Performance validated (no regression)
4. ✅ Tested across multiple queries
5. ✅ Documented which workloads benefit

## Quick Start Investigation

To start investigating now:

```bash
# 1. Run bisection tests (triggers 6 GitHub Actions runs)
./bisect-spark-optimizations.sh

# 2. Wait for results (check in ~2-3 hours)
gh run list --workflow=spark-benchmark.yml --limit=10

# 3. Identify which test shows regression

# 4. Run diagnostics on that flag
./test-with-diagnostics.sh q3

# 5. Download and analyze logs from GitHub Actions artifacts
```

## Summary

- ⚠️ **Flags disabled** due to -2.5% overall regression (q3: -43.9%)
- ✅ **Safe default** configuration in place
- 🔧 **Investigation tools** created and ready
- 🧪 **Manual testing** still possible via workflow inputs
- 📊 **Bisection tests** will identify the problematic flag
- 🐛 **Bug fix** in progress

The optimizations ARE implemented (sophisticated code), they just need debugging/tuning!
