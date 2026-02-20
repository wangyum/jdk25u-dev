# Separate Jobs for True Parallel Execution

## Problem with Background Processes

Running both benchmarks as background processes in one step had issues:
- ❌ `tail -f` commands caused hanging
- ❌ Difficult to debug failures
- ❌ Logs mixed together
- ❌ Can't see individual job progress in GitHub UI

## Solution: Separate GitHub Actions Jobs

Split into **5 independent jobs** that run in parallel where possible:

```
build-jdk-and-spark ──┐
                      ├──> run-baseline-benchmark ──┐
download-dataset ─────┤                             ├──> compare-results
                      ├──> run-optimized-benchmark ─┘
                      └──────────────────────────────────────────────────┘
```

## Job Structure

### 1. `build-jdk-and-spark`
- Builds JDK (20 min)
- Builds Spark (30-60 min)
- Uploads as artifacts
- **Timeout:** 2 hours

### 2. `download-dataset`
- Downloads TPC-DS 5GB dataset
- Uploads as artifact
- **Timeout:** 30 min
- **Runs in parallel with build**

### 3. `run-baseline-benchmark`
- Downloads JDK, Spark, dataset artifacts
- Runs baseline benchmark (no optimizations)
- Uploads results
- **Timeout:** 6 hours
- **Runs in parallel with optimized**

### 4. `run-optimized-benchmark`
- Downloads JDK, Spark, dataset artifacts
- Runs optimized benchmark (with JVM flags)
- Uploads results
- **Timeout:** 6 hours
- **Runs in parallel with baseline**

### 5. `compare-results`
- Downloads both result logs
- Compares query-by-query
- Generates summary
- Uploads comparison
- **Timeout:** 30 min
- **Runs after both benchmarks complete**

## Benefits

### ✅ True Parallel Execution
- baseline and optimized run on **separate runners**
- No shared state whatsoever
- No SBT interference
- Maximum GitHub Actions parallelism

### ✅ Clean GitHub UI
```
Jobs:
  ✅ build-jdk-and-spark (1h 20m)
  ✅ download-dataset (2m)
  ✅ run-baseline-benchmark (45m) ← Running
  ✅ run-optimized-benchmark (42m) ← Running
  ⏳ compare-results (waiting...)
```

### ✅ Independent Failures
- If baseline fails, optimized still runs
- If optimized fails, baseline still runs
- Compare step runs even if one benchmark fails (`if: always()`)

### ✅ Better Resource Usage
- Each job gets its own runner
- Full 14GB disk per job
- No disk space issues
- No copying Spark twice on same machine

### ✅ Easier Debugging
- Each job has its own log
- Clear separation of concerns
- Can re-run individual jobs
- Artifacts uploaded independently

## Artifact Flow

### Uploaded by build-jdk-and-spark:
- `jdk-build-{sha}` - Custom JDK (~500 MB)
- `spark-build-{sha}` - Built Spark (~2 GB)
- Retention: 1 day (temporary)

### Uploaded by download-dataset:
- `tpcds-dataset-{sha}` - TPC-DS 5GB data (~800 MB)
- Retention: 1 day (temporary)

### Uploaded by run-baseline-benchmark:
- `baseline-results-{sha}` - Benchmark log
- Retention: 30 days

### Uploaded by run-optimized-benchmark:
- `optimized-results-{sha}` - Benchmark log
- Retention: 30 days

### Uploaded by compare-results:
- `benchmark-comparison-{sha}` - All logs + comparison
  - benchmark-baseline.log
  - benchmark-optimized.log
  - comparison_output.txt
  - comparison_results.txt
  - compare_results.py
- Retention: 30 days

## Execution Timeline

```
0:00  ┌─ build-jdk-and-spark starts
0:00  ├─ download-dataset starts
0:02  └─ download-dataset completes ✅

1:20  └─ build-jdk-and-spark completes ✅

1:21  ┌─ run-baseline-benchmark starts (downloads artifacts)
1:21  └─ run-optimized-benchmark starts (downloads artifacts)

      [Both run in parallel for ~40-50 minutes]

2:05  ├─ run-optimized-benchmark completes ✅
2:08  └─ run-baseline-benchmark completes ✅

2:09  ├─ compare-results starts
2:10  └─ compare-results completes ✅

Total: ~2 hours 10 minutes
```

## Comparison with Previous Approaches

### Approach 1: Sequential (Original)
```
Build → Baseline → Optimized
Time: ~2 hours (sequential)
```
- ❌ Slow
- ✅ Simple

### Approach 2: Background in same step
```
Build → (Baseline & Optimized in background)
Time: ~1.5 hours
```
- ✅ Parallel
- ❌ tail -f hanging
- ❌ Hard to debug
- ❌ Mixed logs

### Approach 3: Separate directories
```
Build → Copy Spark → (Baseline & Optimized in background)
Time: ~1.5 hours
```
- ✅ Parallel
- ❌ SBT still interferes
- ❌ Disk space issues (3x Spark)
- ❌ Background process management

### Approach 4: Separate jobs (CURRENT)
```
Build ──┬──> Baseline ──┐
Dataset ┴──> Optimized ─┴──> Compare
Time: ~1.5 hours
```
- ✅ True parallel
- ✅ No interference
- ✅ Clean logs
- ✅ Independent failures
- ✅ Easy debugging
- ✅ GitHub UI integration

## Resource Usage Per Job

### build-jdk-and-spark:
- CPU: Heavy (compilation)
- Disk: ~3 GB
- Memory: ~8 GB
- Duration: 1-2 hours

### download-dataset:
- CPU: Light
- Disk: ~1 GB
- Memory: ~1 GB
- Duration: 2-5 minutes

### run-baseline-benchmark:
- CPU: Heavy (benchmark)
- Disk: ~3 GB (Spark + dataset)
- Memory: ~8 GB
- Duration: 40-60 minutes

### run-optimized-benchmark:
- CPU: Heavy (benchmark)
- Disk: ~3 GB (Spark + dataset)
- Memory: ~8 GB
- Duration: 40-60 minutes

### compare-results:
- CPU: Light (Python script)
- Disk: ~100 MB (logs)
- Memory: ~1 GB
- Duration: 1-2 minutes

## Total GitHub Actions Minutes

- build-jdk-and-spark: 80 minutes
- download-dataset: 2 minutes
- run-baseline-benchmark: 50 minutes (parallel)
- run-optimized-benchmark: 50 minutes (parallel)
- compare-results: 2 minutes

**Sequential time:** 184 minutes
**Parallel time:** 132 minutes (baseline & optimized run simultaneously)
**Savings:** 52 minutes per run

## Error Handling

Each job has `continue-on-error: true` or `if: always()` where appropriate:

```yaml
run-baseline-benchmark:
  # If fails, optimized still runs

run-optimized-benchmark:
  # If fails, baseline still runs

compare-results:
  if: always()  # Runs even if benchmarks fail
  steps:
    - name: Download baseline
      continue-on-error: true  # Continue if missing

    - name: Download optimized
      continue-on-error: true  # Continue if missing
```

## Summary

**Before:** One big job with background processes
**After:** 5 small jobs with clear dependencies

**Result:**
- ✅ True parallel execution on separate runners
- ✅ Clean GitHub Actions UI
- ✅ Independent failure handling
- ✅ Easy debugging
- ✅ Proper artifact management
- ✅ 6 hour timeout for benchmarks
- ✅ No SBT/disk/tail -f issues

This is the **proper GitHub Actions way** to run parallel workloads!
