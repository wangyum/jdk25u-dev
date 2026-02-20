# Both Workflows Now Configurable!

## Summary

Both benchmark workflows now support manual configuration via workflow_dispatch inputs.

## Quick Reference

### spark-benchmark.yml
Tests custom JDK optimizations for Spark

**Quick test:**
```bash
gh workflow run spark-benchmark.yml --ref spark -f query_filter="q3"
```

**Defaults:**
- Baseline: `-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch`
- Optimized: `-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization`

### tpcds-benchmark.yml
Compares G1GC vs ZGC

**Quick test:**
```bash
gh workflow run tpcds-benchmark.yml --ref spark -f query_filter="q3"
```

**Defaults:**
- Baseline (G1GC): `-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch`
- Optimized (ZGC): `-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch`

## Common Inputs

Both workflows accept the same 4 inputs:

### 1. baseline_opts
JVM options for baseline benchmark
- spark-benchmark: Standard G1GC
- tpcds-benchmark: G1GC

### 2. optimized_opts
JVM options for optimized benchmark
- spark-benchmark: G1GC + Spark optimizations
- tpcds-benchmark: ZGC

### 3. log_level
Spark log level: `ERROR`, `WARN`, `INFO`
- Default: `ERROR` (clean output)

### 4. query_filter
TPC-DS queries to run (comma-separated)
- Default: `q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96`

## Example Commands

### Test with Single Query (Fast)

```bash
# Test Spark optimizations
gh workflow run spark-benchmark.yml --ref spark -f query_filter="q3"

# Test G1GC vs ZGC
gh workflow run tpcds-benchmark.yml --ref spark -f query_filter="q3"
```

### Test with Custom Heap Size

```bash
# Spark benchmark with 4GB heap
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization"

# TPC-DS benchmark with 4GB heap
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms4g -XX:+UseZGC -XX:+AlwaysPreTouch"
```

### Debug Run

```bash
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f log_level="INFO" \
  -f query_filter="q3"
```

## Workflow Comparison

| Feature | spark-benchmark.yml | tpcds-benchmark.yml |
|---------|---------------------|---------------------|
| **Builds JDK** | ✅ Custom JDK 25 | ❌ Standard JDK 25 |
| **Baseline** | Standard G1GC | G1GC |
| **Optimized** | G1GC + Spark opts | ZGC |
| **Use case** | Test JDK optimizations | Compare GC algorithms |
| **Runtime** | ~2h (JDK build) | ~30min (Spark only) |

## How to Access UI (After Merge to Master)

Currently workflows are on `spark` branch. To enable GitHub Actions UI:

```bash
git checkout master
git merge spark
git push origin master
```

Then go to: `https://github.com/wangyum/jdk25u-dev/actions`

You'll see "Run workflow" button for both workflows.

## Current Workaround (Use CLI)

Since workflows are on `spark` branch, use GitHub CLI:

```bash
# List workflows
gh workflow list

# Run workflow
gh workflow run <workflow-name> --ref spark -f <input>=<value>

# Check run status
gh run list --workflow=<workflow-name>

# Watch run
gh run watch <run-id>
```

## Documentation

Detailed docs for each workflow:

1. **SPARK_BENCHMARK_MANUAL_OPTIONS.md** - spark-benchmark.yml guide
2. **TPCDS_MANUAL_JVM_OPTIONS.md** - tpcds-benchmark.yml guide
3. **WORKFLOW_DISPATCH_FIX.md** - How to enable UI access

## Files Modified

### spark-benchmark.yml
- Added workflow_dispatch with 4 inputs
- Added configuration step
- Updated warmup, baseline, optimized steps

### tpcds-benchmark.yml
- Added workflow_dispatch with 4 inputs
- Added configuration step
- Updated warmup, G1GC, ZGC steps

## Benefits

✅ **No workflow editing** - Configure via inputs
✅ **Quick testing** - Single query validation
✅ **Flexible** - Test any JVM flags
✅ **Safe defaults** - Automatic runs unchanged
✅ **Full control** - Choose queries, logging, options

## Common Use Cases

### 1. Quick Validation

```bash
gh workflow run spark-benchmark.yml --ref spark -f query_filter="q3"
gh workflow run tpcds-benchmark.yml --ref spark -f query_filter="q3"
```

### 2. Test Subset of Queries

```bash
gh workflow run spark-benchmark.yml --ref spark -f query_filter="q3,q7,q19"
```

### 3. Compare Different Heap Sizes

```bash
# Run 1: 3GB
gh workflow run tpcds-benchmark.yml --ref spark

# Run 2: 4GB
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms4g -XX:+UseZGC -XX:+AlwaysPreTouch"
```

### 4. Test Individual Optimization Flags

```bash
# Test only hash optimizations
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1SparkOptimizeHashOperations"
```

### 5. Debug Issues

```bash
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f log_level="INFO" \
  -f query_filter="q3"
```

## Important Notes

**DO:**
- ✅ Keep `-Xms` flag (for AlwaysPreTouch)
- ✅ Use GitHub CLI for now (or merge to master)
- ✅ Start with single query for validation
- ✅ Use ERROR log level for clean output

**DON'T:**
- ❌ Include `-Xmx` (Spark sets it automatically)
- ❌ Add spaces in query_filter (`q1,q2` not `q1, q2`)
- ❌ Expect UI button (until merged to master)

## Next Steps

1. **Test workflows:**
   ```bash
   gh workflow run spark-benchmark.yml --ref spark -f query_filter="q3"
   gh workflow run tpcds-benchmark.yml --ref spark -f query_filter="q3"
   ```

2. **Verify results:**
   ```bash
   gh run list --workflow=spark-benchmark.yml
   gh run list --workflow=tpcds-benchmark.yml
   ```

3. **Enable UI (optional):**
   ```bash
   git checkout master
   git merge spark
   git push origin master
   ```

## Summary

Both workflows are now fully configurable! Test different JVM options, queries, and logging levels without editing workflow files. Use GitHub CLI to trigger with custom configurations.

**Quick start:**
```bash
# Test Spark optimizations
gh workflow run spark-benchmark.yml --ref spark -f query_filter="q3"

# Test G1GC vs ZGC
gh workflow run tpcds-benchmark.yml --ref spark -f query_filter="q3"
```

Happy benchmarking! 🚀
