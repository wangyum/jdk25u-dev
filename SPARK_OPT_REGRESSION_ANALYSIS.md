# Spark Optimization Flags Regression Analysis

## Problem

Benchmark results show that enabling Spark optimization flags makes performance **worse**, not better.

## Benchmark Results Summary

```
TOTAL: 14928ms (baseline) → 15305ms (optimized) = -2.5% slower
```

**Major regressions:**
- q3: -43.9% (426ms → 613ms) ⚠️
- q27: -6.7%
- q73: -8.0%
- q79: -7.8%

**Minor improvements:**
- q43: +5.8%
- q19: +3.1%
- q55: +3.0%
- q96: +0.6%

## Flags Under Test

```bash
-XX:+UnlockExperimentalVMOptions
-XX:+G1OptimizeForSpark
-XX:+G1SparkEnhanceEscapeAnalysis
-XX:+G1SparkOptimizeHashOperations
-XX:+G1SparkEnableVectorization
```

## Investigation Steps

### 1. Verify Flags Are Implemented

Check if these flags actually do anything in the JDK source:

```bash
cd /Users/yumwang/opensource/jdk25u-dev

# Search for flag implementations
git grep -n "G1OptimizeForSpark" src/
git grep -n "G1SparkEnhanceEscapeAnalysis" src/
git grep -n "G1SparkOptimizeHashOperations" src/
git grep -n "G1SparkEnableVectorization" src/

# Check flag definitions
find src/ -name "*.hpp" -o -name "*.cpp" | xargs grep -l "G1Spark"
```

### 2. Check Flag Registration

Look for where flags are defined:

```bash
# Find VM flags definitions
git grep "product.*G1Spark" src/
git grep "experimental.*G1Spark" src/
```

### 3. Test Individual Flags

Test each flag separately to identify which one(s) cause the regression:

```bash
# Test without any optimizations (baseline)
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f query_filter="q3"

# Test with only G1OptimizeForSpark
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark" \
  -f query_filter="q3"

# Test with only G1SparkEnhanceEscapeAnalysis
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1SparkEnhanceEscapeAnalysis" \
  -f query_filter="q3"

# Test with only G1SparkOptimizeHashOperations
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1SparkOptimizeHashOperations" \
  -f query_filter="q3"

# Test with only G1SparkEnableVectorization
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1SparkEnableVectorization" \
  -f query_filter="q3"
```

### 4. Enable JVM Diagnostics

Run with diagnostic output to see what's happening:

```bash
# Add diagnostic flags
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+PrintFlagsFinal" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+PrintFlagsFinal -XX:+PrintCompilation" \
  -f query_filter="q3" \
  -f log_level="WARN"
```

This will show:
- Which flags are actually enabled
- Compilation activity differences
- Any deoptimization events

### 5. Check for Deoptimization

Deoptimization can cause significant slowdowns:

```bash
# Run with deopt tracing
-XX:+UnlockDiagnosticVMOptions -XX:+LogCompilation -XX:+TraceDeoptimization
```

### 6. Profile the Regression

Use async-profiler to see where time is being spent:

```bash
# Compare flame graphs between baseline and optimized
# Look for differences in hot paths
```

## Hypothesis 1: Flags Are Not Implemented

**Test:** Check if flags exist in source code

**If true:** The flags are defined but don't actually do anything (no-op). This would explain no improvement, but not the regression.

## Hypothesis 2: Flags Add Overhead Without Benefit

**Test:** Profile with each flag individually

**If true:** The flags might be:
- Adding instrumentation/checks
- Preventing some other optimizations
- Causing deoptimization

## Hypothesis 3: Flags Have Bugs

**Test:** Review flag implementation code

**If true:** Fix the bugs in the JDK code

## Hypothesis 4: Flags Work But Are Harmful for This Workload

**Test:** Try different query patterns, different data sizes

**If true:** The optimizations might help other workloads but hurt TPC-DS queries

## Immediate Actions

### A. Verify Flag Existence

```bash
cd /Users/yumwang/opensource/jdk25u-dev

# Check if any of these flags exist
git grep "G1Spark" src/ | head -20

# If no results, the flags might not be implemented yet!
```

### B. Test Without Optimizations

Verify baseline vs baseline (should be same performance):

```bash
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f query_filter="q3,q7,q19"
```

### C. Focus on q3 Regression

q3 shows -43.9% regression, making it a good test case:

```bash
# Run q3 multiple times to confirm consistency
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f query_filter="q3"
```

## Expected Outcomes

### If flags are not implemented:
- Performance should be identical (or minor variance)
- But we see -2.5% overall, suggesting some effect

### If flags add overhead:
- Profile will show additional time in GC or runtime checks
- PrintCompilation might show deoptimization

### If flags have bugs:
- Code review will reveal issues
- Specific queries might trigger bad behavior

## Decision Tree

```
Are flags implemented in source code?
├─ NO → Implement them or remove from workflow
└─ YES → Do they add overhead?
    ├─ YES → Profile to find overhead source
    │   ├─ Instrumentation? → Remove or optimize
    │   ├─ Deoptimization? → Fix conditions causing deopt
    │   └─ Extra GC work? → Fix GC logic
    └─ NO → Are they working as intended?
        ├─ YES → Workload doesn't benefit, disable flags
        └─ NO → Fix implementation bugs
```

## Next Steps

1. **Check flag implementation** in JDK source
2. **Test each flag individually** on q3 (worst regression)
3. **Add diagnostic output** to understand what's happening
4. **Profile both runs** to see time distribution differences
5. **Review code** if flags exist

## Recommendation

Based on -2.5% overall regression, I recommend:

1. **Disable these flags for now** in the default configuration
2. **Investigate flag implementations**
3. **Test individually** to isolate the problematic flag(s)
4. **Fix or remove** flags that cause regressions

Update `spark-benchmark.yml` defaults to just use standard G1GC for both baseline and optimized until flags are proven to help.
