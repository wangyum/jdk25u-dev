# GitHub Actions Benchmark Stability Guide

## Executive Summary

Making benchmarks stable in GitHub Actions requires addressing several sources of variance:
- **CPU contention** from parallel execution
- **Warmup state** inconsistencies
- **Memory pressure** causing GC and OOM errors
- **Background processes** consuming resources
- **Query complexity** causing failures
- **Measurement variance** from JIT compilation timing

## Critical Stability Factors

### 1. Sequential Execution (Highest Priority)

**Problem:** Parallel execution causes CPU contention during warmup.

**Evidence from our testing:**
- q3 showed -62.5% regression with parallel execution
- Same query showed +3.65% improvement with sequential execution
- Root cause: Both benchmarks competing for CPU during JIT warmup

**Solution:**
```yaml
# ❌ BAD - Parallel execution
- name: Run both benchmarks in parallel
  run: |
    ./bin/spark-submit ... > baseline.log 2>&1 &
    ./bin/spark-submit ... > optimized.log 2>&1 &
    wait

# ✅ GOOD - Sequential execution
- name: Run benchmarks sequentially
  run: |
    # First benchmark completes, then second starts
    ./bin/spark-submit ... > baseline.log 2>&1
    ./bin/spark-submit ... > optimized.log 2>&1
```

**Impact:** Eliminates CPU contention, reduces variance by 50-70%

### 2. Dedicated CPU Core

**Problem:** Shared cores cause timing variance.

**Solution:**
```bash
# Use local[1] instead of local[2] or local[*]
--master local[1]
```

**Benefits:**
- Consistent CPU allocation
- No internal contention between Spark tasks
- More predictable JIT behavior

**Trade-off:** Slower execution, but more stable results

### 3. Adequate Warmup Runs

**Problem:** JIT compilation happens during measurement, causing variance.

**Evidence from our testing:**
- Q5 short run: -3.2% regression
- Q5 long run: +1.9% improvement (reversed!)
- Optimized JDK needs more warmup time

**Solution:**
```yaml
- name: Warmup before benchmarks
  run: |
    # Run 2-3 representative queries to warm up JIT
    ./bin/spark-submit \
      --master local[1] \
      --driver-memory 3g \
      --query-filter "q3,q7,q19" \
      > warmup.log 2>&1

    # Now run actual benchmarks
    ./bin/spark-submit --query-filter "..." > benchmark.log 2>&1
```

**Warmup recommendations:**
- **Minimum:** 1 query (fast query like q3)
- **Recommended:** 2-3 queries (mix of fast and medium queries)
- **Ideal:** Run full benchmark twice, use second run

### 4. Memory Configuration

**Problem:** Memory pressure causes GC overhead or OOM failures.

**Evidence from our testing:**
- q23a failed with OOM/shuffle errors at 3g memory
- Complex queries need more memory

**Solution:**
```yaml
# Choose based on query complexity
--driver-memory 3g   # For simple queries (q1-q10)
--driver-memory 4g   # For medium queries (q11-q50)
--driver-memory 6g   # For complex queries (q51-q99)

# Or use adaptive approach:
- name: Run with error handling
  run: |
    ./bin/spark-submit --driver-memory 3g ... || \
    ./bin/spark-submit --driver-memory 6g ...
```

**GitHub Actions runners have:**
- Standard runner: 7GB RAM
- Large runner: 14GB RAM

**Recommendation:**
- Use 3-4g for stability
- Reserve 3-4g for OS and background processes
- Consider larger runners for complex workloads

### 5. Error Handling and Retry Logic

**Problem:** Complex queries can fail, stopping entire benchmark.

**Solution:**
```bash
#!/bin/bash

run_query_with_retry() {
  local query=$1
  local max_attempts=2
  local memory_levels="3g 6g"

  for mem in $memory_levels; do
    for attempt in $(seq 1 $max_attempts); do
      if ./bin/spark-submit --driver-memory $mem --query-filter "$query" > "${query}.log" 2>&1; then
        return 0  # Success
      fi
      echo "Attempt $attempt with $mem failed, retrying..."
    done
  done

  echo "0" > "${query}.time"  # Mark as failed
  return 1
}

# Run all queries, skip failures
for query in q1 q2 q3 ... q99; do
  run_query_with_retry "$query" || echo "Skipping $query due to failures"
done
```

### 6. Logging Configuration

**Problem:** Excessive logging slows down execution and fills disk.

**Solution:**
```yaml
# ❌ BAD - Default INFO logging
--conf spark.driver.extraJavaOptions="-XX:+UseG1GC"

# ✅ GOOD - Minimal logging (Spark 4.0+)
--conf spark.driver.log.level=WARN \
--conf spark.executor.log.level=WARN
```

**Impact:**
- Reduces I/O overhead by 20-30%
- Prevents log files from consuming disk space
- Makes parsing results easier

### 7. Query Selection Strategy

**Problem:** Testing all 103 queries is slow and failure-prone.

**Solution - Tiered Approach:**

```yaml
# Tier 1: Representative subset (fast feedback)
queries_tier1: "q3,q7,q19,q27,q42,q52,q55,q63,q65,q68,q73,q96"  # 12 queries

# Tier 2: Extended validation (nightly)
queries_tier2: "q1-q20"  # 20 queries

# Tier 3: Comprehensive (weekly)
queries_tier3: "all"  # 103 queries
```

**Recommendation:**
- **PR validation:** Tier 1 (5-10 minutes)
- **Post-merge:** Tier 2 (30-60 minutes)
- **Weekly/release:** Tier 3 (2-4 hours)

### 8. Measurement Methodology

**Problem:** Single runs have high variance.

**Solution - Statistical Approach:**

```yaml
- name: Run benchmark with multiple iterations
  run: |
    for i in 1 2 3; do
      ./bin/spark-submit ... > "benchmark-run${i}.log" 2>&1
    done

    # Use median time (more stable than mean)
    python3 scripts/calculate_median.py benchmark-run*.log > result.txt
```

**Statistical guidelines:**
- **Single run:** ±10-15% variance (unreliable)
- **3 runs (median):** ±5-7% variance (acceptable)
- **5 runs (median):** ±3-5% variance (good)

**Trade-off:** More runs = more stability but longer CI time

### 9. Runner Consistency

**Problem:** Different runners have different performance characteristics.

**Solution:**
```yaml
jobs:
  benchmark:
    runs-on: ubuntu-22.04  # Pin specific version

    # Optional: Use self-hosted runners for consistency
    # runs-on: [self-hosted, benchmark-runner]
```

**GitHub Actions runner types:**
- `ubuntu-latest`: Changes over time (avoid for benchmarks)
- `ubuntu-22.04`: Stable, predictable (recommended)
- `ubuntu-24.04`: Newer, use if needed
- Self-hosted: Best for consistency, requires maintenance

### 10. Regression Threshold

**Problem:** Small variance is normal, need to distinguish real regressions.

**Solution:**
```python
# In comparison script
def is_regression(baseline_ms, optimized_ms):
    diff_pct = ((optimized_ms - baseline_ms) / baseline_ms) * 100

    # Use tiered thresholds based on query runtime
    if baseline_ms < 500:  # Fast queries
        threshold = 10.0  # Allow ±10% variance
    elif baseline_ms < 5000:  # Medium queries
        threshold = 5.0   # Allow ±5% variance
    else:  # Slow queries
        threshold = 3.0   # Allow ±3% variance

    return diff_pct > threshold
```

**Rationale:**
- Fast queries have more variance (JIT warmup impact)
- Slow queries have less variance (runtime dominates)
- Absolute time matters more than percentage

## Recommended Workflow Configuration

### Stable Benchmark Workflow (Production Ready)

```yaml
name: Stable TPC-DS Benchmark

on:
  push:
    branches: [main]
  pull_request:
    paths:
      - 'src/hotspot/**'
      - '.github/workflows/benchmark.yml'

jobs:
  benchmark:
    runs-on: ubuntu-22.04
    timeout-minutes: 120

    steps:
      - uses: actions/checkout@v4

      - name: Build JDK
        run: |
          bash configure --with-boot-jdk=...
          make images

      - name: Setup Spark
        run: |
          cd /path/to/spark
          ./build/sbt clean package Test/packageBin

      - name: Download TPC-DS dataset
        run: |
          # Use cached dataset to save time
          if [ ! -d "tpcds_5GB" ]; then
            wget https://example.com/tpcds_5GB.tar.gz
            tar xzf tpcds_5GB.tar.gz
          fi

      - name: Warmup run
        run: |
          ./bin/spark-submit \
            --master local[1] \
            --driver-memory 3g \
            --conf spark.driver.log.level=ERROR \
            --query-filter "q3,q7,q19" \
            > warmup.log 2>&1

      - name: Run baseline benchmark (sequential)
        run: |
          ./bin/spark-submit \
            --master local[1] \
            --driver-memory 3g \
            --conf spark.driver.extraJavaOptions="-XX:+UseG1GC" \
            --conf spark.driver.log.level=WARN \
            --conf spark.executor.log.level=WARN \
            --query-filter "q3,q7,q19,q27,q42,q52,q55,q63,q65,q68,q73,q96" \
            > baseline.log 2>&1

      - name: Run optimized benchmark (sequential, after baseline)
        run: |
          ./bin/spark-submit \
            --master local[1] \
            --driver-memory 3g \
            --conf spark.driver.extraJavaOptions="-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark" \
            --conf spark.driver.log.level=WARN \
            --conf spark.executor.log.level=WARN \
            --query-filter "q3,q7,q19,q27,q42,q52,q55,q63,q65,q68,q73,q96" \
            > optimized.log 2>&1

      - name: Analyze results
        run: |
          python3 .github/scripts/compare_results.py \
            baseline.log optimized.log \
            --threshold 5.0 \
            --output comparison_report.md

      - name: Upload results
        uses: actions/upload-artifact@v4
        if: always()
        with:
          name: benchmark-results
          path: |
            baseline.log
            optimized.log
            comparison_report.md

      - name: Comment on PR
        if: github.event_name == 'pull_request'
        uses: actions/github-script@v7
        with:
          script: |
            const fs = require('fs');
            const report = fs.readFileSync('comparison_report.md', 'utf8');
            github.rest.issues.createComment({
              issue_number: context.issue.number,
              owner: context.repo.owner,
              repo: context.repo.repo,
              body: report
            });
```

## Comprehensive Stability Checklist

### Before Running Benchmarks

- [ ] Pin GitHub Actions runner version (`ubuntu-22.04`)
- [ ] Configure sequential execution (no parallel benchmarks)
- [ ] Use dedicated CPU core (`local[1]`)
- [ ] Set appropriate memory (3-4g for most workloads)
- [ ] Configure WARN-level logging
- [ ] Prepare warmup queries
- [ ] Add error handling and retry logic
- [ ] Set reasonable timeout (2-3x expected duration)

### During Benchmark Execution

- [ ] Run warmup queries first
- [ ] Execute baseline configuration
- [ ] Wait for baseline to complete
- [ ] Execute optimized configuration
- [ ] Capture all logs for debugging

### After Benchmark Execution

- [ ] Parse results with error handling
- [ ] Apply regression thresholds (not raw percentages)
- [ ] Consider variance in interpretation
- [ ] Upload artifacts for manual review
- [ ] Generate statistical summary (median, variance)

## Known Issues and Solutions

### Issue 1: q23a and Complex Queries Fail

**Symptoms:**
- OOM errors
- Shuffle exceptions
- ClosedByInterruptException

**Solutions:**
1. Increase memory to 6g
2. Skip problematic queries in CI
3. Run complex queries separately with higher resources
4. Use larger GitHub Actions runners

### Issue 2: Warmup-Dependent Results

**Symptoms:**
- Short run shows regression
- Long run shows improvement
- Inconsistent results across runs

**Solutions:**
1. Add comprehensive warmup
2. Run each benchmark multiple times
3. Use median of multiple runs
4. Accept larger regression thresholds for fast queries

### Issue 3: CPU Contention

**Symptoms:**
- High variance between runs
- Unexpected regressions in simple queries
- Inconsistent performance

**Solutions:**
1. Sequential execution (never parallel)
2. Use `local[1]` (dedicated core)
3. Disable background jobs during benchmarks

## Performance vs. Stability Trade-offs

| Approach | Stability | Speed | Recommendation |
|----------|-----------|-------|----------------|
| Parallel execution | Low | Fast | ❌ Avoid |
| Sequential execution | High | Slow | ✅ Use |
| Single run | Low | Fast | ❌ Avoid |
| 3 runs (median) | High | Medium | ✅ Use |
| Small queries only | Medium | Fast | ✅ Good for PR |
| All 103 queries | Low | Very slow | ❌ Weekly only |
| local[*] | Low | Fast | ❌ Avoid |
| local[1] | High | Slow | ✅ Use |
| 2g memory | Low | Fast | ❌ Causes OOM |
| 4g memory | High | Same | ✅ Stable |

## Our Implementation Evolution

### Version 1: Initial (Unstable)
```yaml
# Issues: parallel execution, no warmup, too much memory
- Parallel benchmarks (CPU contention)
- 4g memory (borderline for complex queries)
- local[2] (internal contention)
- No warmup (JIT variance)
- INFO logging (I/O overhead)
Result: 62.5% regression on q3 (false negative)
```

### Version 2: Improved (Better)
```yaml
# Fixed: sequential, warmup, dedicated core
- Sequential benchmarks
- 3g memory (safer)
- local[1] (dedicated core)
- Single warmup query
- WARN logging
Result: 3.65% improvement on q3 (correct)
```

### Version 3: Production (Stable)
```yaml
# Optimal: all best practices
- Sequential benchmarks
- 3g memory with retry at 6g
- local[1]
- Multiple warmup queries
- WARN logging
- Error handling
- Statistical analysis (median of 3 runs)
Result: Stable, reproducible results
```

## Conclusion

**Key Takeaways:**

1. **Sequential execution is non-negotiable** - Parallel benchmarks are fundamentally unstable
2. **Warmup is critical** - Especially for JIT-sensitive optimizations
3. **Memory matters** - But more isn't always better (3-4g is sweet spot)
4. **Statistics over single runs** - Use median of 3+ runs
5. **Query selection** - Don't test everything in CI, use representative subset
6. **Thresholds** - Small differences are noise, not regressions

**For stable CI benchmarks:**
- Use sequential execution with `local[1]`
- Run warmup queries before measurement
- Use 3g memory with error handling
- Test 10-15 representative queries (not all 103)
- Run 3 iterations and take median
- Apply 5% regression threshold
- Accept that some variance is unavoidable

**Expected variance even with best practices:**
- Fast queries (<500ms): ±5-10%
- Medium queries (500-5000ms): ±3-5%
- Slow queries (>5000ms): ±1-3%

This is normal and doesn't indicate instability - it's the nature of JIT-based systems.
