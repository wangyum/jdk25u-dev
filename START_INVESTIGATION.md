# Quick Start: Investigate Spark Optimization Regression

## What Happened

Spark optimization flags cause **-2.5% overall slowdown** (q3 shows **-43.9% regression**)

## What's Been Done

✅ Flags **disabled by default** in `spark-benchmark.yml`
✅ Investigation tools **created and ready**
✅ Root cause **analysis documented**

## Start Investigation NOW

### Option 1: Run Full Bisection (Recommended)

This will identify which flag causes the regression:

```bash
cd /Users/yumwang/opensource/jdk25u-dev
./bisect-spark-optimizations.sh
```

**What it does:**
- Triggers 6 GitHub Actions workflow runs
- Tests each flag individually
- Takes ~2-3 hours total
- Results show which flag is the culprit

**After completion:**
```bash
# Check results
gh run list --workflow=spark-benchmark.yml --limit=10

# Download artifacts
gh run download <run-id>
```

### Option 2: Test Single Flag Quickly

If you suspect a specific flag:

```bash
# Test escape analysis
./test-single-flag.sh G1SparkEnhanceEscapeAnalysis q3

# Test hash operations
./test-single-flag.sh G1SparkOptimizeHashOperations q3

# Test vectorization
./test-single-flag.sh G1SparkEnableVectorization q3

# Test G1 tuning only
./test-single-flag.sh G1OptimizeForSpark q3
```

### Option 3: Run with Diagnostics

Get detailed JVM diagnostics to understand what's happening:

```bash
./test-with-diagnostics.sh q3
```

**Diagnostic output includes:**
- GC activity logs
- JIT compilation logs
- Deoptimization events
- Performance counters

## Expected Results

### Bisection Tests

You'll see which test shows regression:

| Test | Configuration | Expected Time |
|------|---------------|---------------|
| 0 | Baseline (no opts) | ~426ms ✅ |
| 1 | G1OptimizeForSpark only | ??? |
| 2 | + EscapeAnalysis | ??? |
| 3 | + HashOps | ??? |
| 4 | + Vectorization | ??? |
| 5 | All opts | ~613ms ❌ |

**If Test 1 is slow:** G1 tuning itself is the problem
**If Test 2 is slow:** Escape analysis causes it
**If Test 3 is slow:** Hash optimization causes it
**If Test 4 is slow:** Vectorization causes it

## Manual Workflow Triggers

### Test baseline vs baseline (sanity check)
```bash
gh workflow run spark-benchmark.yml --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f query_filter="q3"
```
**Expected:** Both should be ~426ms

### Test with all optimizations (confirm regression)
```bash
gh workflow run spark-benchmark.yml --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization" \
  -f query_filter="q3"
```
**Expected:** Optimized should be ~613ms (regression confirmed)

## Check Workflow Status

```bash
# List recent runs
gh run list --workflow=spark-benchmark.yml --limit=10

# Watch a specific run
gh run watch <run-id>

# View logs
gh run view <run-id> --log

# Download artifacts
gh run download <run-id>
```

## Analyze Results

After tests complete:

1. **Compare times** - Which test shows regression?
2. **Check logs** - Any deoptimization events?
3. **Review code** - Check implementation of problematic flag
4. **Fix or tune** - Either fix bugs or adjust parameters

## Likely Culprits

Based on implementation review:

### Most Likely: G1SparkEnhanceEscapeAnalysis
- Complex escape analysis logic
- Targets Spark InternalRow objects
- Could be too aggressive
- May cause deoptimization

**Check:** Does Test 2 show regression?

### Second Likely: G1OptimizeForSpark (G1 Tuning)
- Changes multiple G1GC parameters
- YoungGen: 10%-70% (vs default 5%-60%)
- IHOP: 30% (vs default 45%)
- Reserve: 15% (vs default 10%)

**Check:** Does Test 1 show regression?

### Less Likely: G1SparkOptimizeHashOperations
- Assembly-optimized MurmurHash3
- Platform-specific code
- Should be faster, not slower

**Check:** Does Test 3 show regression?

### Least Likely: G1SparkEnableVectorization
- SIMD vectorization
- Should help or have no effect

**Check:** Does Test 4 show regression?

## Next Steps After Identification

Once you know which flag causes the regression:

### If Escape Analysis:
```bash
# Check implementation
cat src/hotspot/share/gc/shared/sparkEscapeAnalysisOptimizer.cpp

# Look for thresholds
git grep "G1SparkScalarReplacementThreshold\|G1SparkStackAllocationLimit"

# Try tuning
gh workflow run spark-benchmark.yml --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:G1SparkScalarReplacementThreshold=100"
```

### If G1 Tuning:
```bash
# Check parameters
cat src/hotspot/share/gc/g1/g1_globals.hpp | grep "G1Spark"

# Try different values
gh workflow run spark-benchmark.yml --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:G1SparkYoungGenMinPercent=5 -XX:G1SparkYoungGenMaxPercent=60"
```

### If Hash Operations:
```bash
# Check assembly implementation
cat src/hotspot/cpu/aarch64/stubGenerator_aarch64.cpp | grep -A50 "generate_sparkMurmur3Hash"
cat src/hotspot/cpu/x86/stubGenerator_x86_64.cpp | grep -A50 "generate_sparkMurmur3Hash"

# Disable just hash ops
gh workflow run spark-benchmark.yml --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:-G1SparkOptimizeHashOperations"
```

## Files to Review

**Optimization implementations:**
- `src/hotspot/share/gc/shared/sparkEscapeAnalysisOptimizer.cpp`
- `src/hotspot/share/gc/shared/sparkVectorizationOptimizer.cpp`
- `src/hotspot/cpu/aarch64/stubGenerator_aarch64.cpp` (hash ops)
- `src/hotspot/cpu/x86/stubGenerator_x86_64.cpp` (hash ops)

**Flag definitions:**
- `src/hotspot/share/gc/g1/g1_globals.hpp`

**G1GC policy:**
- `src/hotspot/share/gc/g1/g1Policy.cpp`

## Summary

**Start with:**
```bash
./bisect-spark-optimizations.sh
```

**Wait 2-3 hours, then:**
```bash
gh run list --workflow=spark-benchmark.yml --limit=10
```

**Analyze results** to identify the problematic flag

**Fix or tune** the problematic optimization

**Re-enable** flags after validation

Good luck! 🔍
