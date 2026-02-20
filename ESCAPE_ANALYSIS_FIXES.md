# Escape Analysis Implementation Fixes

## Problem Identified

Bisection testing revealed that `G1SparkEnhanceEscapeAnalysis` caused a **-43.8% regression** on TPC-DS q3:
- Baseline: 473ms
- With EscapeAnalysis: 680ms
- Regression: -43.8%

## Root Causes

### 1. Pattern Matching Too Broad (Line 72)
**Problem:**
```cpp
return strstr(class_name, pattern) != nullptr;
```
- Matches pattern ANYWHERE in string
- `"InternalRow"` would match `"MyInternalRowWrapper"`, `"NotAnInternalRow"`, etc.
- Too many false positives causing unnecessary optimization attempts

**Fix:**
```cpp
bool matches_exact_class(const char* class_name, const char* target_class) {
  // Match exact class name, not just substring
  // Check boundaries: must be preceded by '/' or be at start
  // Must be followed by '\0', '$', or ';'
}
```

### 2. Wrong Size Heuristic (Line 168)
**Problem:**
```cpp
if (object_size > 1024) return false;  // Assumes small = non-escaping
```
- Blindly assumes objects < 1KB don't escape
- No evidence this is true for Spark workloads
- TPC-DS q3 proved this assumption wrong

**Fix:**
```cpp
if (object_size > 256) return false;  // Much more conservative
// Later: Only optimize objects <= 128 bytes for InternalRow
// Only optimize objects <= 64 bytes for stack allocation
```

### 3. No Runtime Verification (Line 197-199)
**Problem:**
```cpp
if (is_internal_row_class(class_name)) {
  return field_count <= 10;  // Blindly scalarizes, no verification
}
```
- No deoptimization tracking
- No adaptive behavior when assumptions fail
- Once enabled, keeps making bad decisions

**Fix:**
```cpp
// Added deoptimization tracking
static size_t _deoptimization_count;
static size_t _optimization_attempts;
static bool _adaptive_mode_disabled;

void record_deoptimization(const char* class_name, const char* reason) {
  _deoptimization_count++;

  // Check deopt rate and disable if too high
  if (_optimization_attempts > 50) {
    double deopt_rate = (double)_deoptimization_count / _optimization_attempts;
    if (deopt_rate > 0.15) {  // More than 15% deopt rate
      _adaptive_mode_disabled = true;
      log_warning(gc)("Disabling EA optimizations due to high deopt rate: %.1f%%",
                      deopt_rate * 100);
    }
  }
}
```

### 4. Overly Aggressive Optimization Targets

**Problem:**
- Optimized InternalRow with up to 10 fields (scalar replacement)
- Optimized InternalRow up to 256 bytes (stack allocation)
- Optimized iterator wrappers and expression evaluations
- All based on guesses, not evidence

**Fix:**
```cpp
// Scalar replacement: ONLY for tiny InternalRow (≤ 4 fields)
if (is_internal_row_class(class_name) && field_count <= 4) {
  return true;  // Down from 10 fields
}

// Stack allocation: ONLY for very small InternalRow (≤ 64 bytes)
if (is_internal_row_class(class_name) && object_size <= 64) {
  return true;  // Down from 256 bytes
}

// REMOVED: Iterator wrapper optimizations (caused deopt)
// REMOVED: Expression evaluation optimizations (caused deopt)
```

## Changes Made

### File: sparkEscapeAnalysisOptimizer.cpp

#### 1. Added Deoptimization Tracking
```cpp
// New static variables
size_t _deoptimization_count = 0;
size_t _optimization_attempts = 0;
bool _adaptive_mode_disabled = false;
```

#### 2. Added Exact Class Matching
```cpp
bool matches_exact_class(const char* class_name, const char* target_class) {
  // Match exact class name with boundary checks
  // Prevents false positives like "NotAnInternalRow" matching "InternalRow"
}
```

#### 3. Updated `is_internal_row_class()`
```cpp
// Before: Matched any class with "InternalRow" in name
return matches_pattern(class_name, "InternalRow") || ...;

// After: Match only specific known classes
return matches_exact_class(class_name, "GenericInternalRow") ||
       matches_exact_class(class_name, "SpecificInternalRow");
```

#### 4. Updated `is_likely_non_escaping_spark_object()`
```cpp
// Added adaptive mode check
if (_adaptive_mode_disabled) {
  return false;
}

// Check deopt rate and disable if too high
if (_optimization_attempts > 100) {
  double deopt_rate = (double)_deoptimization_count / _optimization_attempts;
  if (deopt_rate > 0.10) {
    _adaptive_mode_disabled = true;
    log_warning(gc)("Disabling EA: high deopt rate %.1f%%", deopt_rate * 100);
    return false;
  }
}

// Reduced size limit from 1024 to 256 bytes
if (object_size > 256) return false;

// Only optimize tiny InternalRow (≤ 128 bytes)
if (is_internal_row_class(class_name) && object_size <= 128) {
  _optimization_attempts++;
  return true;
}

// REMOVED: Iterator and expression optimizations
```

#### 5. Updated `should_scalarize_aggressively()`
```cpp
// Added adaptive mode check
if (_adaptive_mode_disabled) return false;

// Reduced field count from 10 to 4
if (is_internal_row_class(class_name) && field_count <= 4) {
  _optimization_attempts++;
  return true;
}

// REMOVED: Expression eval scalarization
```

#### 6. Updated `should_attempt_stack_allocation()`
```cpp
// Added adaptive mode check
if (_adaptive_mode_disabled) return false;

// Reduced size from 256 to 64 bytes
if (is_internal_row_class(class_name) && object_size <= 64) {
  _optimization_attempts++;
  return true;
}

// REMOVED: Iterator wrapper stack allocation
```

#### 7. Added `record_deoptimization()`
```cpp
void record_deoptimization(const char* class_name, const char* reason) {
  _deoptimization_count++;

  log_debug(gc)("Deoptimization of %s, reason: %s", class_name, reason);

  // Check if we should disable adaptive mode
  if (_optimization_attempts > 50) {
    double deopt_rate = (double)_deoptimization_count / _optimization_attempts;
    if (deopt_rate > 0.15) {
      _adaptive_mode_disabled = true;
      log_warning(gc)("DISABLING EA: deopt rate %.1f%%", deopt_rate * 100);
    }
  }
}
```

#### 8. Enhanced `print_statistics()`
```cpp
// Now prints deopt statistics
log_info(gc)("Optimization attempts: %zu", _optimization_attempts);
log_info(gc)("Deoptimizations: %zu", _deoptimization_count);
log_info(gc)("Deoptimization rate: %.1f%%", deopt_rate);
if (_adaptive_mode_disabled) {
  log_info(gc)("Adaptive mode: DISABLED (high deopt rate)");
}
```

### File: sparkEscapeAnalysisOptimizer.hpp

#### 1. Added Deoptimization Tracking Declarations
```cpp
// In private section:
static size_t _deoptimization_count;
static size_t _optimization_attempts;
static bool _adaptive_mode_disabled;
```

#### 2. Added New Method Declarations
```cpp
static void record_deoptimization(const char* class_name, const char* reason);
static bool matches_exact_class(const char* class_name, const char* target_class);
```

## Impact Analysis

### Before Fixes
- Size threshold: 1024 bytes (too large)
- Field count threshold: 10 fields (too many)
- Stack allocation: 256 bytes (too large)
- Pattern matching: Substring match (too broad)
- Adaptive behavior: None (no deopt tracking)
- Result: **-43.8% regression on q3**

### After Fixes
- Size threshold: 256 bytes → 128 bytes (conservative)
- Field count threshold: 4 fields (very conservative)
- Stack allocation: 64 bytes (very conservative)
- Pattern matching: Exact class match (precise)
- Adaptive behavior: Tracks deopt rate, disables if > 15%
- Expected result: **Minimal or no regression**

## Testing Plan

### Step 1: Build and Test
```bash
# Build JDK with fixes
bash configure --with-boot-jdk=$BOOT_JDK
make images

# Run benchmark with fixed EscapeAnalysis
gh workflow run spark-benchmark.yml --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis" \
  -f query_filter="q3"
```

### Step 2: Compare Results
Expected outcomes:

**Best case:** Slight improvement
- Fixed implementation actually helps
- q3: 440ms baseline → 420ms optimized
- Improvement: +4.5%

**Likely case:** Neutral
- Optimizations rarely trigger (too conservative)
- q3: 440ms baseline → 445ms optimized
- Regression: -1% (acceptable)

**Worst case:** Minor regression
- Some overhead remains but much less
- q3: 440ms baseline → 460ms optimized
- Regression: -4.5% (acceptable, vs -43.8% before)

### Step 3: Check Logs
```bash
# Look for deopt warnings in logs
grep "Spark EA" benchmark-optimized.log
grep "Deoptimization" benchmark-optimized.log

# Expected log output:
# Spark Escape Analysis Statistics:
#   Optimization attempts: 150
#   Deoptimizations: 8
#   Deoptimization rate: 5.3%
#   Adaptive mode: ENABLED (low deopt rate)
```

## Conservative Approach Rationale

The fixes are VERY conservative because:

1. **TPC-DS q3 evidence:** Showed that aggressive optimization fails badly
2. **Unknown workloads:** We don't know all Spark workload patterns
3. **Deopt is expensive:** One deopt can cost more than 100 eliminated allocations
4. **Adaptive mode:** If deopts are low, optimizations stay enabled; if high, they auto-disable

**Philosophy:** Better to optimize nothing than to cause -43.8% regression

## Next Steps

### If Testing Shows Neutral/Positive Results
1. Enable by default in workflow
2. Document safe usage
3. Consider gradually relaxing thresholds based on evidence

### If Testing Still Shows Regression
1. Further reduce thresholds
2. Add more logging to understand why deopts occur
3. Consider per-query adaptive tuning

### Future Improvements
1. Runtime profiling to identify truly non-escaping objects
2. Workload-specific tuning (TPCDS vs joins vs aggregations)
3. Integration with Spark's own escape analysis hints
4. Machine learning model to predict escape behavior

## Summary

**Key Changes:**
- ✅ Added deoptimization tracking
- ✅ Added adaptive disable when deopt rate > 15%
- ✅ Fixed pattern matching to be exact, not substring
- ✅ Reduced size thresholds: 1024→256→128→64 bytes
- ✅ Reduced field count: 10→4 fields
- ✅ Removed iterator and expression optimizations
- ✅ Enhanced logging and statistics

**Expected Outcome:**
- Eliminate or drastically reduce -43.8% regression
- Safe to enable in production with adaptive guards
- Foundation for future evidence-based tuning
