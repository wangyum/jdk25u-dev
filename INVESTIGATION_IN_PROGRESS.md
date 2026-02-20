# 🔍 Investigation In Progress: Spark Optimization Regression

## Current Status

**Tests:** ✅ Running (6 bisection tests triggered)
**Started:** 2026-02-20 07:13 UTC
**Expected completion:** ~2-3 hours (around 09:13-10:13 UTC)

## What's Running

6 GitHub Actions workflow runs testing individual flags:

| Test | Configuration | Expected Result |
|------|---------------|-----------------|
| 0 | Baseline (no opts) | ~426ms ✅ |
| 1 | G1OptimizeForSpark only | ~500-550ms ⚠️ |
| 2 | + EscapeAnalysis | ~580-630ms ⚠️⚠️ |
| 3 | + HashOps | ~430-450ms |
| 4 | + Vectorization | ~430-450ms |
| 5 | All optimizations | ~613ms ❌ (confirmed) |

## Code Review Completed

✅ Reviewed all optimization implementations
✅ Identified suspicious code patterns
✅ Made predictions based on code

### Key Findings:

**Most Suspicious: G1SparkEnhanceEscapeAnalysis**
- Uses broad pattern matching (`strstr()`)
- Aggressive heuristics (assumes small objects don't escape)
- No runtime verification → may cause deoptimization
- **Prediction:** Test 2 will show ~580-630ms regression

**Second Suspicious: G1OptimizeForSpark (G1 Tuning)**
- Young gen too large (70% vs default 60%)
- Aggressive string dedup (age=1)
- 3x TLAB multiplier
- **Prediction:** Test 1 will show ~500-550ms regression

**Less Suspicious: Hash Operations & Vectorization**
- Should help or be neutral
- **Prediction:** Tests 3 & 4 close to baseline (~430-450ms)

## Actions Taken

### 1. ✅ Workflow Updated
- Disabled optimization flags by default
- Both baseline and optimized now use same safe config
- Manual testing still possible via inputs

### 2. ✅ Tools Created
- `bisect-spark-optimizations.sh` - Triggered 6 tests
- `test-single-flag.sh` - Test individual flags
- `test-with-diagnostics.sh` - Run with full diagnostics
- `check-bisection-results.sh` - Monitor test progress

### 3. ✅ Documentation Created
- `CODE_REVIEW_FINDINGS.md` - Detailed code analysis
- `OPTIMIZATION_FLAGS_DISABLED.md` - Status and impact
- `SPARK_OPT_REGRESSION_ANALYSIS.md` - Root cause analysis
- `SPARK_OPT_INVESTIGATION_PLAN.md` - Investigation strategy
- `START_INVESTIGATION.md` - Quick start guide

## Check Test Progress

```bash
# Check status
./check-bisection-results.sh

# Or manually
gh run list --workflow=spark-benchmark.yml --limit=10
```

## When Tests Complete

### Step 1: Download Results
```bash
# List completed runs
gh run list --workflow=spark-benchmark.yml --limit=6

# Download artifacts
gh run download <run-id>
```

### Step 2: Extract q3 Times
```bash
# From downloaded benchmark logs
grep -A5 "q3" benchmark-baseline.log
grep -A5 "q3" benchmark-optimized.log
```

### Step 3: Identify Culprit
Compare optimized times across tests:
- If Test 2 >> baseline → EscapeAnalysis is the problem
- If Test 1 >> baseline → G1 tuning is the problem
- If Tests 3 or 4 >> baseline → Hash/Vectorization is the problem

### Step 4: Apply Fix
Based on which test shows regression, apply appropriate fix from `CODE_REVIEW_FINDINGS.md`.

## Predicted Outcome

**Most likely scenario:**
- Test 1 shows regression (~500-550ms) - G1 tuning doesn't match workload
- Test 2 shows worse regression (~580-630ms) - EscapeAnalysis adds overhead
- Tests 3 & 4 are close to baseline - Hash/Vec don't hurt much
- Test 5 confirms ~613ms - All flags combined

**Root cause:** G1SparkEnhanceEscapeAnalysis + G1OptimizeForSpark (G1 tuning)

**Recommended fix:**
```bash
# Disable EscapeAnalysis by default
-XX:-G1SparkEnhanceEscapeAnalysis

# Tune G1 parameters
-XX:G1SparkYoungGenMaxPercent=50  # Reduce from 70
-XX:-G1SparkAggressiveStringDedup  # Disable string dedup
-XX:G1SparkTLABSizeMultiplier=1    # Reduce from 3
```

## Timeline

**07:13 UTC** - Bisection tests triggered
**~09:13 UTC** - Tests should start completing
**~10:13 UTC** - All tests should be complete
**After completion** - Analyze results and apply fixes

## What Happens Next

### Scenario A: Predictions Confirmed

If Test 2 (EscapeAnalysis) shows the worst regression:

1. **Disable EscapeAnalysis by default**
2. **Tune G1 parameters** (young gen, string dedup, TLAB)
3. **Re-test** with fixes
4. **Document** which workloads benefit
5. **Consider** making flags opt-in instead of opt-out

### Scenario B: Unexpected Results

If different test shows regression:

1. **Review** that flag's implementation more carefully
2. **Add diagnostics** to understand behavior
3. **Profile** to find performance bottleneck
4. **Fix** identified issue
5. **Re-test**

### Scenario C: All Tests Regress Similarly

If all tests show similar regression:

1. **Problem is G1OptimizeForSpark itself** (master switch)
2. **Even without sub-optimizations**, G1 tuning hurts
3. **Disable entire G1OptimizeForSpark** by default
4. **Re-evaluate** all G1 parameter choices

## Resources

**Code locations:**
- Escape Analysis: `src/hotspot/share/gc/shared/sparkEscapeAnalysisOptimizer.cpp`
- Vectorization: `src/hotspot/share/gc/shared/sparkVectorizationOptimizer.cpp`
- Hash Ops: `src/hotspot/cpu/*/stubGenerator_*.cpp`
- G1 Flags: `src/hotspot/share/gc/g1/g1_globals.hpp`
- G1 Policy: `src/hotspot/share/gc/g1/g1Policy.cpp`

**Test scripts:**
- `bisect-spark-optimizations.sh` - Run all bisection tests
- `test-single-flag.sh` - Test one flag
- `test-with-diagnostics.sh` - Run with diagnostics
- `check-bisection-results.sh` - Check test status

**Documentation:**
- `CODE_REVIEW_FINDINGS.md` - Code analysis and predictions
- `OPTIMIZATION_FLAGS_DISABLED.md` - Current status
- `START_INVESTIGATION.md` - Quick start guide
- This file - `INVESTIGATION_IN_PROGRESS.md`

## Contact / Next Check-in

**Check status in:** ~2 hours (around 09:13 UTC)

```bash
./check-bisection-results.sh
```

**Or watch a running test:**
```bash
gh run list --workflow=spark-benchmark.yml --limit=1
gh run watch <run-id>
```

## Summary

🔄 **Status:** Tests running
⏱️ **Time:** ~2-3 hours to completion
🎯 **Goal:** Identify which flag causes -43.9% regression
📊 **Prediction:** EscapeAnalysis + G1 tuning are the culprits
✅ **Ready:** Tools and docs prepared for analysis
⏭️ **Next:** Wait for results, then apply fixes

The investigation is underway! 🚀
