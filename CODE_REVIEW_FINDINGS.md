# Code Review Findings: Spark Optimization Flags

## Status: Bisection Tests Running

6 tests triggered at: 2026-02-20 07:13-07:14 UTC
Expected completion: ~2-3 hours

## Code Review Summary

I've reviewed the implementation code while tests run. Here's what I found:

### 1. G1SparkEnhanceEscapeAnalysis (MOST SUSPICIOUS)

**Implementation:** `src/hotspot/share/gc/shared/sparkEscapeAnalysisOptimizer.cpp`

**What it does:**
- Pattern-matches Spark SQL class names (InternalRow, Iterator, Expression, etc.)
- Attempts aggressive optimization based on heuristics
- Three optimization strategies:
  1. **Scalar replacement** - Split objects into individual fields
  2. **Stack allocation** - Allocate on stack instead of heap
  3. **Allocation elimination** - Remove allocation entirely

**Suspicious Code Patterns:**

#### Size Heuristic (Line 168):
```cpp
// Size heuristic: small objects (< 1KB) are more likely to be optimized
if (object_size > 1024) {
    return false;
}
```
⚠️ This is just a guess! May misidentify objects.

#### Aggressive Scalarization (Line 197-199):
```cpp
if (is_internal_row_class(class_name)) {
    // InternalRow with few fields: excellent candidate
    return field_count <= 10;
}
```
⚠️ Blindly scalarizes all InternalRow with ≤10 fields. May cause deoptimization if assumption is wrong.

#### Stack Allocation Threshold (Line 225):
```cpp
if (is_internal_row_class(class_name) && object_size <= 256) {
    // Small InternalRow instances
    return true;
}
```
⚠️ Assumes all small InternalRows don't escape. This could be wrong!

**Potential Issues:**

1. **Pattern matching too broad**
   - Uses `strstr()` to match class names (line 72)
   - Could match unintended classes
   - Example: Matches ANY class containing "InternalRow" in path

2. **Heuristics may be wrong**
   - Assumes small size = non-escaping
   - Assumes few fields = good for scalarization
   - May trigger deoptimization when assumptions fail

3. **No runtime verification**
   - Once optimization is applied, if object escapes → deoptimization
   - Deoptimization is EXPENSIVE

4. **Overly aggressive for query workloads**
   - May work for streaming but not batch queries
   - TPC-DS q3 might have different patterns than expected

### 2. G1OptimizeForSpark (SECOND SUSPICIOUS)

**Changes multiple G1GC parameters:**

| Parameter | Default | G1Spark | Impact |
|-----------|---------|---------|---------|
| Young Gen Min | 5% | **10%** | Larger young gen |
| Young Gen Max | 60% | **70%** | Even larger young gen |
| IHOP | 45% | **30%** | Earlier concurrent marking |
| Reserve % | 10% | **15%** | More reserved memory |
| String Dedup | false | **true** | String deduplication active |
| String Dedup Age | 3 | **1** | Very aggressive dedup |
| TLAB Multiplier | 1x | **3x** | 3x larger TLABs |

**Potential Issues:**

1. **Too large young gen (70%)**
   - More young gen = more copying during young GC
   - If allocation rate is high, young GC could be slower
   - May not match TPC-DS query patterns

2. **Earlier concurrent marking (30% IHOP)**
   - Starts marking earlier
   - More overhead from concurrent work
   - May not be needed for batch queries

3. **Aggressive string dedup (age=1)**
   - Dedups strings at age 1 (very young)
   - Overhead of dedup hash table lookups
   - If queries don't have many duplicate strings → wasted work

4. **3x TLAB size**
   - Larger TLABs = less TLAB refills
   - But also = more wasted space in partially-filled TLABs
   - May increase memory fragmentation

**Why this could cause q3 regression:**

q3 might have:
- Low allocation rate → large TLABs wasted
- Few duplicate strings → string dedup overhead for no gain
- Batch processing → young gen too large, slowing young GC

### 3. G1SparkOptimizeHashOperations (LESS SUSPICIOUS)

**Implementation:** Assembly code in:
- `src/hotspot/cpu/aarch64/stubGenerator_aarch64.cpp:11859`
- `src/hotspot/cpu/x86/stubGenerator_x86_64.cpp:4308`

**What it does:**
- Provides optimized MurmurHash3 implementation
- Used for Spark's hash-based operations (joins, aggregations)

**Why it's less suspicious:**
- Hash operations should be faster, not slower
- Assembly code is platform-specific and well-tested
- Unless there's a bug, should help or be neutral

**Potential issue:**
- If there's a bug in the assembly, could cause crashes or slowdowns
- But unlikely to cause -43.9% regression

### 4. G1SparkEnableVectorization (LEAST SUSPICIOUS)

**Implementation:** `src/hotspot/share/gc/shared/sparkVectorizationOptimizer.cpp`

**What it does:**
- Attempts SIMD vectorization for array operations
- Auto-vectorization for loops

**Why it's least suspicious:**
- Vectorization either works or falls back to scalar
- Fallback shouldn't be slower than not trying at all
- Unless there's overhead in detection/decision logic

## Predicted Results

Based on code review, I predict:

### Test 0 (Baseline): ~426ms ✅
No optimizations, baseline performance.

### Test 1 (G1OptimizeForSpark only): **LIKELY REGRESSION**
**Prediction:** ~500-550ms (-17% to -29%)

Why: G1GC tuning parameters may not match TPC-DS q3 workload.
- 70% young gen too large
- String dedup overhead
- 3x TLAB wasted

### Test 2 (+ EscapeAnalysis): **LIKELY WORSE REGRESSION**
**Prediction:** ~580-630ms (-36% to -48%)

Why: Escape analysis adds overhead on top of bad G1 tuning.
- Pattern matching overhead
- Deoptimization when assumptions fail
- Analysis time wasted

**This is the most likely culprit for q3's -43.9% regression!**

### Test 3 (+ HashOperations): ~430-450ms (-1% to -6%)
**Prediction:** Slight regression or neutral

Why: Hash ops should help, but if query doesn't use much hashing → overhead of checking flags.

### Test 4 (+ Vectorization): ~430-450ms (-1% to -6%)
**Prediction:** Slight regression or neutral

Why: Vectorization detection overhead, but fallback to scalar should be close to baseline.

### Test 5 (All optimizations): ~613ms ✅ (confirmed)
We know this regresses to 613ms.

## Root Cause Hypothesis

**Primary culprit:** G1SparkEnhanceEscapeAnalysis
- Too aggressive pattern matching
- Wrong assumptions for batch query workload
- Deoptimization overhead

**Secondary culprit:** G1OptimizeForSpark (G1 tuning)
- Young gen too large (70%)
- String dedup overhead for no gain
- 3x TLAB wasted space

**Combined effect:** -43.9% regression

## Recommended Fixes

### If Test 2 shows regression (EscapeAnalysis):

**Option 1: Disable by default**
```bash
-XX:-G1SparkEnhanceEscapeAnalysis
```

**Option 2: Tune thresholds**
```cpp
// More conservative thresholds
G1SparkScalarReplacementThreshold: 20 → 10
G1SparkStackAllocationLimit: 512 → 256
```

**Option 3: Add runtime guards**
- Track deoptimization rate
- Disable if deopt rate > threshold
- Adaptive optimization

### If Test 1 shows regression (G1 tuning):

**Option 1: Tune young gen**
```
G1SparkYoungGenMaxPercent: 70 → 50
```

**Option 2: Disable string dedup**
```
G1SparkAggressiveStringDedup: true → false
```

**Option 3: Reduce TLAB multiplier**
```
G1SparkTLABSizeMultiplier: 3 → 1
```

## Next Steps

1. **Wait for test results** (~2-3 hours)
2. **Identify which test(s) show regression**
3. **Apply recommended fixes** based on results
4. **Re-test** with fixes
5. **Re-enable flags** once validated

## Test Status Tracking

Check status with:
```bash
gh run list --workflow=spark-benchmark.yml --limit=10
```

When complete, download artifacts:
```bash
gh run download <run-id>
```

Look for benchmark logs containing q3 results for each test.

## Summary

**Most likely culprit:** G1SparkEnhanceEscapeAnalysis (Test 2)
- Overly aggressive for batch queries
- Pattern matching too broad
- Deoptimization overhead

**Second culprit:** G1OptimizeForSpark (Test 1)
- G1GC tuning doesn't match workload
- Young gen too large
- String dedup wasted

**Expected outcome:** Tests 1 and 2 will show regression, Tests 3 and 4 will be close to baseline.

Waiting for test results to confirm! 🔍
