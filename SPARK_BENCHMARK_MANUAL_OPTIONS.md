# Spark Benchmark Manual JVM Options

## Overview

The `spark-benchmark.yml` workflow now supports manual configuration of JVM options through GitHub Actions workflow_dispatch inputs.

## Default Configuration

When triggered by push (automatic):

- **Baseline (Standard G1GC):** `-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch`
- **Optimized (Spark optimizations):** `-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization`
- **Log Level:** `ERROR`
- **Query Filter:** `q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96`

## What This Workflow Tests

This workflow compares:
- **Baseline:** Standard G1GC without custom optimizations
- **Optimized:** G1GC with custom Spark-specific JVM optimizations

Both run on a custom-built JDK 25 with potential performance enhancements.

## Manual Trigger Inputs

### 1. baseline_opts
- **Description:** JVM options for baseline (standard G1GC)
- **Default:** `-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch`
- **Example:** `-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:G1HeapRegionSize=4m`

### 2. optimized_opts
- **Description:** JVM options for optimized (G1GC + Spark optimizations)
- **Default:** `-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization`
- **Example:** Add more experimental flags

### 3. log_level
- **Description:** Spark logging level
- **Options:** `ERROR`, `WARN`, `INFO`
- **Default:** `ERROR`

### 4. query_filter
- **Description:** TPC-DS queries to run (comma-separated)
- **Default:** `q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96`
- **Example:** `q3` for quick testing

## How to Use

### Via GitHub CLI

**Quick test (single query):**
```bash
gh workflow run spark-benchmark.yml --ref spark -f query_filter="q3"
```

**Test baseline with different heap:**
```bash
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f query_filter="q3,q7,q19"
```

**Test new Spark optimization flags:**
```bash
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization -XX:+MyNewOptimization"
```

**Disable all Spark optimizations (baseline only):**
```bash
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch"
```

**Full custom configuration:**
```bash
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization" \
  -f query_filter="q3,q7,q19,q27,q42,q43" \
  -f log_level="ERROR"
```

## Workflow Structure

This workflow:

1. **Builds custom JDK 25** with potential optimizations
2. **Builds Apache Spark** (java25 branch)
3. **Warmup:** Uses `baseline_opts` for JIT warmup
4. **BASELINE benchmark:** Standard G1GC (uses `baseline_opts`)
5. **OPTIMIZED benchmark:** G1GC + Spark optimizations (uses `optimized_opts`)
6. **Comparison:** Automatically compares results

Each benchmark outputs to separate files:
- Baseline: `benchmark-baseline.log`
- Optimized: `benchmark-optimized.log`

## Output Display

Configuration is displayed at start:

```
==========================================
Benchmark Configuration
==========================================
Baseline opts:   -Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch
Optimized opts:  -Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization
Log level:       ERROR
Query filter:    q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96
==========================================
```

Each benchmark shows:

```
==========================================
TPC-DS Benchmark - BASELINE
==========================================
Configuration: -Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch
Queries: q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96
Start: Thu Feb 20 12:34:56 UTC 2026
```

## Example Use Cases

### 1. Quick Validation (Single Query)

Test the workflow runs correctly:

```bash
gh workflow run spark-benchmark.yml --ref spark -f query_filter="q3"
```

**Runtime:** ~30 minutes (including JDK build)

### 2. Test Individual Spark Optimization Flags

Test if `G1SparkOptimizeHashOperations` helps:

```bash
# Baseline without any optimizations
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1SparkOptimizeHashOperations"
```

### 3. Test All Optimizations vs Baseline

Run full comparison with default settings:

```bash
gh workflow run spark-benchmark.yml --ref spark
```

### 4. Isolate Escape Analysis Impact

Compare with and without enhanced escape analysis:

```bash
# Run 1: Without escape analysis
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization"

# Run 2: With escape analysis
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+G1OptimizeForSpark -XX:+G1SparkEnhanceEscapeAnalysis -XX:+G1SparkOptimizeHashOperations -XX:+G1SparkEnableVectorization"
```

### 5. Debug with Verbose Logging

Enable INFO logging to debug issues:

```bash
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f log_level="INFO" \
  -f query_filter="q3"
```

## Spark Optimization Flags Explained

The optimized configuration includes these custom JDK flags:

### G1OptimizeForSpark
Master flag that enables Spark-specific G1GC optimizations.

### G1SparkEnhanceEscapeAnalysis
Enhanced escape analysis for Spark workloads:
- Better object allocation optimization
- More aggressive scalar replacement
- Improved stack allocation decisions

### G1SparkOptimizeHashOperations
Optimized hash operations for Spark:
- Faster hash map operations
- Better hash distribution
- Optimized for Spark's hash-based operations

### G1SparkEnableVectorization
Vectorization optimizations for Spark:
- SIMD operations where applicable
- Better loop unrolling for Spark patterns
- Optimized array operations

## Important Notes

### 1. Keep -Xms Flag

Always include `-Xms3g` (or your chosen heap size) for AlwaysPreTouch to work properly.

### 2. Don't Include -Xmx

Spark sets `-Xmx` automatically from `--driver-memory`. Including it causes an error.

### 3. UnlockExperimentalVMOptions Required

All custom Spark optimization flags require `-XX:+UnlockExperimentalVMOptions` first.

### 4. Testing New Flags

When testing new experimental JVM flags:

```bash
gh workflow run spark-benchmark.yml \
  --ref spark \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+MyNewFlag"
```

## Comparison with tpcds-benchmark.yml

| Feature | spark-benchmark.yml | tpcds-benchmark.yml |
|---------|---------------------|---------------------|
| **Builds JDK** | ✅ Yes (custom JDK 25) | ❌ No (uses standard JDK 25) |
| **Baseline** | Standard G1GC | G1GC |
| **Optimized** | G1GC + Spark opts | ZGC |
| **Purpose** | Test Spark-specific JVM optimizations | Compare G1GC vs ZGC |
| **Build time** | ~2 hours (JDK build) | ~30 minutes (Spark only) |

Use `spark-benchmark.yml` to test JDK optimizations, use `tpcds-benchmark.yml` to compare GC algorithms.

## Verification

After running, check:

1. Configuration displayed correctly
2. JDK built with custom flags
3. Both benchmarks completed
4. Comparison results generated
5. Artifacts uploaded

## Artifacts

Results uploaded as: `benchmark-results-<git-sha>`

Contains:
- `benchmark-baseline.log`
- `benchmark-optimized.log`
- `comparison_output.txt`
- `comparison_results.txt`

## Troubleshooting

**Problem:** JDK build fails

**Solution:** Check the JDK source code compiles. This workflow builds JDK from source.

---

**Problem:** Optimization flags not recognized

**Solution:** Ensure flags are implemented in your JDK fork. Standard JDK 25 doesn't have these flags.

---

**Problem:** No performance difference

**Solution:**
- Custom flags may not be implemented yet
- Workload may not trigger optimizations
- Try different queries

## Summary

The Spark benchmark workflow tests custom JDK optimizations for Spark workloads. It's fully configurable for:
- Testing individual optimization flags
- A/B testing different configurations
- Quick validation with single queries
- Debugging with verbose logging

Use GitHub CLI to trigger with custom configurations without editing workflow files!
