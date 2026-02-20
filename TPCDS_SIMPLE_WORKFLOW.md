# Simple TPC-DS Benchmark Workflow

## Overview

A simplified workflow that runs TPC-DS benchmarks using standard JDK 21, without custom JDK builds or performance comparisons.

## Workflow File

`.github/workflows/tpcds-benchmark.yml`

## What It Does

1. **Uses JDK 21** - Standard Temurin distribution (no custom build)
2. **Builds Spark** - From java25 branch with SBT
3. **Downloads TPC-DS dataset** - Pre-generated 5GB dataset
4. **Runs benchmark** - Single execution with standard G1GC
5. **Uploads results** - Saves benchmark log as artifact

## Execution Flow

```
tpcds-benchmark (Single Job)
├─ Install JDK 21 (~1 min)
├─ Build Spark with SBT (~30 min)
├─ Download TPC-DS dataset (~2 min)
├─ Run TPC-DS benchmark (~40 min)
└─ Upload results

Total: ~73 minutes
```

## Key Features

### Simple and Fast ✅
- No custom JDK build (saves ~20 minutes)
- Single benchmark run (not comparing anything)
- Standard JDK 21 (widely available)

### Uses spark-submit ✅
```bash
./bin/spark-submit \
  --master local[*] \
  --driver-memory 4g \
  --conf spark.driver.extraJavaOptions="-XX:+UseG1GC" \
  --jars "$SPARK_CATALYST_TEST_JAR" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3,q7,..."
```

### Runs 14 TPC-DS Queries ✅
- q3, q7, q19, q27, q42, q43, q52, q55
- q63, q65, q68, q73, q79, q96

### Automatic Test Jar Creation ✅
```bash
./build/sbt clean package Test/packageBin
```
- Creates test jars without running tests
- Fast and reliable

## Comparison with spark-benchmark.yml

### spark-benchmark.yml (Complex)
```
Purpose: Compare custom JDK optimizations vs standard JDK
Steps:
  1. Build custom JDK (~20 min)
  2. Build Spark (~30 min)
  3. Run baseline benchmark (~40 min)
  4. Run optimized benchmark (~40 min)
  5. Compare results
Total: ~130 minutes
```

### tpcds-benchmark.yml (Simple)
```
Purpose: Just run TPC-DS benchmark with JDK 21
Steps:
  1. Use JDK 21 (pre-installed)
  2. Build Spark (~30 min)
  3. Run benchmark (~40 min)
Total: ~73 minutes
```

**Savings:** 57 minutes per run

## When to Use Each Workflow

### Use tpcds-benchmark.yml when:
- ✅ Testing Spark changes
- ✅ Validating TPC-DS performance
- ✅ Quick benchmark runs
- ✅ Don't need JDK optimization comparison
- ✅ Want faster results

### Use spark-benchmark.yml when:
- ✅ Testing JDK optimizations
- ✅ Comparing baseline vs optimized
- ✅ Need detailed performance comparison
- ✅ Validating custom JDK changes

## Triggers

```yaml
on:
  push:
    branches:
      - main
      - spark
  workflow_dispatch:
```

**Runs on:**
- Push to main or spark branches
- Manual trigger via GitHub Actions UI

## Artifacts

**Uploaded:**
- `tpcds-benchmark-results-{sha}` - Benchmark log file
- Retention: 30 days

**Contents:**
- Query execution times
- Full benchmark output
- Start/end timestamps

## Resource Usage

**Single Runner:**
- Disk: ~3 GB (Spark + dataset)
- Memory: ~8 GB peak
- CPU: All cores (local[*])

**Runtime:**
- ~73 minutes total
- ~40 minutes for actual benchmark

## Benefits

### Cost Effective ✅
- Single job, single runner
- No custom JDK build overhead
- 44% faster than full comparison workflow

### Simple Debugging ✅
- Single job log
- Standard JDK (reproducible anywhere)
- Clear output

### Good for Development ✅
- Fast iteration on Spark changes
- Standard environment
- Easy to understand

## Configuration

### JDK Settings
```yaml
- uses: actions/setup-java@v4
  with:
    distribution: 'temurin'
    java-version: '21'
```

### Spark Build
```yaml
./build/sbt clean package Test/packageBin
```

### Benchmark Config
```yaml
--master local[*]           # Use all cores
--driver-memory 4g          # 4GB driver memory
--conf spark.driver.extraJavaOptions="-XX:+UseG1GC"
```

## Output Example

```
========================================
TPC-DS Benchmark with JDK 21
========================================
Start: Thu Feb 18 10:00:00 UTC 2026

Running query q3...
q3: 1234 ms

Running query q7...
q7: 2345 ms

...

End: Thu Feb 18 10:40:00 UTC 2026
========================================
```

## Summary

**Purpose:** Quick TPC-DS benchmarking with standard JDK 21

**Time:** ~73 minutes

**Use case:** Testing Spark changes, validating performance

**Benefits:**
- ✅ Fast (44% faster than full workflow)
- ✅ Simple (standard JDK, no comparisons)
- ✅ Cost effective (single runner)
- ✅ Easy to understand and maintain

**File:** `.github/workflows/tpcds-benchmark.yml`

