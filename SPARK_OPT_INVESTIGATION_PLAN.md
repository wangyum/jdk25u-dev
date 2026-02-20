# Spark Optimization Flags Investigation Plan

## Key Finding

✅ **The flags ARE implemented** - They're not no-ops!

The JDK has sophisticated Spark-specific optimization code:
- `sparkEscapeAnalysisOptimizer.cpp` - Enhanced escape analysis for Spark objects
- `sparkVectorizationOptimizer.cpp` - Vectorization for Spark operations
- `stubGenerator_*.cpp` - Optimized MurmurHash3 for Spark
- Multiple G1GC tuning parameters for Spark workloads

## But... Performance is Worse

**Baseline:** 14928ms
**Optimized:** 15305ms (-2.5% slower)

**Worst regression:** q3 at -43.9% (426ms → 613ms)

## Why Might This Happen?

### Theory 1: Optimization Overhead > Benefits

The optimizations might:
- Add profiling/instrumentation overhead
- Trigger deoptimization in certain code paths
- Interfere with JIT compiler's own optimizations
- Use more memory, causing more GC

### Theory 2: Bugs in Implementation

Since this is experimental code (2026 copyright), there might be bugs:
- Incorrect escape analysis decisions
- Broken vectorization logic
- Hash optimization bugs

### Theory 3: Workload Mismatch

The optimizations might target:
- Different Spark versions
- Different query patterns
- Different data sizes
- Streaming workloads vs batch queries

## Diagnostic Strategy

### Step 1: Test Each Flag Individually

Isolate which flag(s) cause the regression:

```bash
# Test 1: Just G1OptimizeForSpark (master switch)
gh workflow run spark-benchmark.yml --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark" \
  -f query_filter="q3"

# Test 2: G1OptimizeForSpark + Escape Analysis ONLY
gh workflow run spark-benchmark.yml --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:-G1SparkOptimizeHashOperations -XX:-G1SparkEnableVectorization" \
  -f query_filter="q3"

# Test 3: G1OptimizeForSpark + Hash Optimization ONLY
gh workflow run spark-benchmark.yml --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:-G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:-G1SparkEnableVectorization" \
  -f query_filter="q3"

# Test 4: G1OptimizeForSpark + Vectorization ONLY
gh workflow run spark-benchmark.yml --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:-G1SparkEnhanceEscapeAnalysis -XX:-G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization" \
  -f query_filter="q3"
```

### Step 2: Enable Diagnostic Logging

See what the optimizations are actually doing:

```bash
# Add logging flags to see optimization activity
gh workflow run spark-benchmark.yml --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+UnlockDiagnosticVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization -Xlog:gc*=info:file=/tmp/gc.log -XX:+PrintCompilation -XX:+LogCompilation" \
  -f query_filter="q3" \
  -f log_level="WARN"
```

### Step 3: Check for Deoptimization

Deoptimization can cause major slowdowns:

```bash
gh workflow run spark-benchmark.yml --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+UnlockDiagnosticVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization -XX:+TraceDeoptimization -XX:+PrintDeoptimizationDetails" \
  -f query_filter="q3"
```

### Step 4: Compare GC Behavior

Check if optimizations cause more GC:

```bash
# Baseline GC stats
-Xlog:gc*=info:file=/tmp/baseline-gc.log

# Optimized GC stats
-Xlog:gc*=info:file=/tmp/optimized-gc.log

# Compare:
# - Number of GC cycles
# - GC pause times
# - Memory allocation rate
# - Object survival patterns
```

## Quick Test: Disable All Optimizations

Confirm the regression goes away when all flags are disabled:

```bash
# Both should have same performance
gh workflow run spark-benchmark.yml --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f query_filter="q3,q7,q19,q27,q42,q43"
```

If performance is identical, confirms the flags are causing the regression.

## Root Cause Candidates

### 1. G1SparkEnhanceEscapeAnalysis

**What it does:**
- Analyzes InternalRow, JoinedRow, MutableProjection allocations
- Attempts scalar replacement (split objects into fields)
- Tries stack allocation for escaping objects

**Potential issues:**
- Might be too aggressive, causing deoptimization
- Stack allocation might fail, wasting analysis time
- Scalar replacement might prevent other optimizations

**Test:** Disable and retest q3

### 2. G1SparkOptimizeHashOperations

**What it does:**
- Provides optimized MurmurHash3 implementation in assembly
- Used for Spark's hash-based operations

**Potential issues:**
- Assembly code might have bugs
- Might not be faster on all CPUs
- Could be interfering with inlining

**Test:** Disable and retest q3

### 3. G1SparkEnableVectorization

**What it does:**
- Attempts SIMD vectorization for Spark operations

**Potential issues:**
- Vectorization might fail, adding overhead
- Might prevent other optimizations
- Could cause alignment issues

**Test:** Disable and retest q3

### 4. G1OptimizeForSpark (G1GC Tuning)

**What it does:**
- Changes G1GC parameters:
  - `G1SparkYoungGenMinPercent: 10%`
  - `G1SparkYoungGenMaxPercent: 70%`
  - `G1SparkInitiatingHeapOccupancyPercent: 30%`
  - `G1SparkReservePercent: 15%`
  - `G1SparkAggressiveStringDedup: true`

**Potential issues:**
- Tuning might not match workload
- Too aggressive young gen might cause more GC
- String dedup overhead might exceed benefits

**Test:** Disable just G1OptimizeForSpark, keep other flags off

## Immediate Action Items

### 1. Bisect the Flags (Priority: HIGH)

Run 4 tests to find the culprit:

```bash
# Test A: Only G1OptimizeForSpark
-XX:+G1OptimizeForSpark (but default values for sub-flags)

# Test B: + EscapeAnalysis
-XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis

# Test C: + HashOps
-XX:+G1OptimizeForSpark -XX:+G1SparkOptimizeHashOperations

# Test D: + Vectorization
-XX:+G1OptimizeForSpark -XX:+G1SparkEnableVectorization
```

### 2. Check Escape Analysis Stats (Priority: MEDIUM)

The code tracks statistics. Add logging to see them:

```cpp
// In sparkEscapeAnalysisOptimizer.cpp, there are counters:
_total_allocations_eliminated
_scalar_replacements
_stack_allocations
_internal_row_eliminations
_iterator_eliminations
_expression_eliminations
_bytes_saved
```

These would show if escape analysis is actually eliminating allocations.

### 3. Review Recent Changes (Priority: MEDIUM)

Check git history for recent changes to these files:

```bash
git log --oneline --since="2 weeks ago" src/hotspot/share/gc/shared/spark*.cpp
git log --oneline --since="2 weeks ago" src/hotspot/share/gc/g1/g1_globals.hpp
```

## Decision Matrix

| Finding | Action |
|---------|--------|
| One flag causes all regression | Disable that flag by default |
| Multiple flags contribute | Disable the worst offenders |
| G1GC tuning is the problem | Adjust G1Spark* parameters |
| Escape analysis too aggressive | Lower thresholds or disable |
| Hash optimization has bugs | Fix assembly code |
| Vectorization fails | Add better detection/fallback |

## Recommended Next Steps

**Immediate (Today):**
1. Run q3 test with each flag individually
2. Identify which flag(s) cause the -43.9% regression

**Short-term (This Week):**
1. Add diagnostic logging to see what optimizations trigger
2. Profile both runs to find time differences
3. Review implementation code for obvious bugs

**Medium-term (Next Week):**
1. Fix identified bugs
2. Tune parameters (thresholds, limits)
3. Add better guards/fallbacks
4. Re-benchmark with fixes

## Expected Outcome

After investigation, we should either:
- **Fix the bugs** and see improvements
- **Tune the parameters** to avoid overhead
- **Disable problematic flags** until fixed
- **Document which workloads benefit** and enable selectively

The goal is to either make the optimizations work or disable them gracefully.
