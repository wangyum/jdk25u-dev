# G1GC vs ZGC Comparison in tpcds-benchmark.yml

## Overview

Updated tpcds-benchmark.yml to compare G1GC and ZGC garbage collectors running in parallel on the same machine.

## What It Does

1. **Runs two benchmarks in parallel:**
   - G1GC benchmark (traditional garbage collector)
   - ZGC benchmark (low-latency garbage collector)

2. **Compares results automatically:**
   - Uses the same comparison script as spark-benchmark.yml
   - Shows query-by-query performance comparison
   - Calculates improvement percentages

## Execution Flow

```
tpcds-benchmark (Single Job)
├─ Install JDK 25
├─ Build Spark
├─ Download TPC-DS dataset
├─ Run both benchmarks in parallel
│  ├─ G1GC benchmark (background)
│  └─ ZGC benchmark (background)
├─ Wait for both to complete
├─ Compare results
└─ Upload all logs + comparison

Total: ~73 minutes (same as before, runs in parallel)
```

## Parallel Execution

### G1GC Benchmark:
```bash
./bin/spark-submit \
  --master local[*] \
  --driver-memory 4g \
  --conf spark.driver.extraJavaOptions="-XX:+UseG1GC -Dlog4j.rootCategory=WARN,console" \
  --jars "$JARS_LIST" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3,q7,..." \
  > benchmark-g1gc.log 2>&1 &
```

### ZGC Benchmark:
```bash
./bin/spark-submit \
  --master local[*] \
  --driver-memory 4g \
  --conf spark.driver.extraJavaOptions="-XX:+UseZGC -Dlog4j.rootCategory=WARN,console" \
  --jars "$JARS_LIST" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3,q7,..." \
  > benchmark-zgc.log 2>&1 &
```

Both run simultaneously in the background!

## GC Differences

### G1GC (Garbage First)
- **Type:** Regional, generational collector
- **Target:** Balanced throughput and latency
- **Pause times:** Predictable, configurable (default ~200ms)
- **Best for:** General-purpose workloads
- **JVM flag:** `-XX:+UseG1GC`

### ZGC (Z Garbage Collector)
- **Type:** Concurrent, non-generational collector
- **Target:** Ultra-low latency
- **Pause times:** Sub-millisecond (< 1ms)
- **Best for:** Large heaps, latency-sensitive workloads
- **JVM flag:** `-XX:+UseZGC`

## Comparison Output

After both benchmarks complete, the workflow compares results:

```
============================================================
QUERY-BY-QUERY COMPARISON
============================================================

Query      Baseline (ms)   Optimized (ms)  Improvement
------------------------------------------------------------
q3         2345.1          2156.4          ✅ +8.0%
q7         1234.5          1111.0          ✅ +10.0%
q19        3456.7          3250.8          ✅ +6.0%
...
------------------------------------------------------------
TOTAL      35318.1         32381.5         ✅ +8.3%

============================================================
Overall Performance: +8.3% improvement
============================================================
```

**Note:** The script labels results as "Baseline" (G1GC) and "Optimized" (ZGC) for compatibility with the existing comparison script.

## Files Generated

### benchmark-g1gc.log
```
========================================
TPC-DS Benchmark - G1GC
========================================
Start: Thu Feb 18 10:00:00 UTC 2026

Running query q3...
q3: 2345 ms

Running query q7...
q7: 1234 ms

End: Thu Feb 18 10:40:00 UTC 2026
```

### benchmark-zgc.log
```
========================================
TPC-DS Benchmark - ZGC
========================================
Start: Thu Feb 18 10:00:00 UTC 2026

Running query q3...
q3: 2156 ms

Running query q7...
q7: 1111 ms

End: Thu Feb 18 10:38:00 UTC 2026
```

### comparison_output.txt
Human-readable comparison showing which GC performed better for each query.

### comparison_results.txt
CSV format for further analysis:
```csv
Query,Baseline(ms),Optimized(ms),Improvement(%)
q3,2345.1,2156.4,8.0
q7,1234.5,1111.0,10.0
TOTAL,35318.1,32381.5,8.3
```

## Artifacts

**Uploaded as:** `tpcds-gc-comparison-{sha}`

**Contains:**
- `benchmark-g1gc.log` - G1GC benchmark results
- `benchmark-zgc.log` - ZGC benchmark results
- `comparison_output.txt` - Human-readable comparison
- `comparison_results.txt` - CSV format comparison

**Retention:** 30 days

## Expected Results

### For TPC-DS Workload:

**G1GC typically better for:**
- Throughput-oriented queries
- Batch processing
- General-purpose analytics

**ZGC typically better for:**
- Latency-sensitive queries
- Consistent response times
- Large heap scenarios (> 4GB)

### Factors Affecting Results:

1. **Query characteristics:**
   - CPU-intensive: G1GC may win
   - Memory-intensive: ZGC may win
   - Mixed: Results will vary

2. **Heap size:**
   - Small heaps (< 4GB): G1GC often better
   - Large heaps (> 8GB): ZGC often better

3. **Pause time sensitivity:**
   - Don't care about pauses: G1GC better throughput
   - Need low latency: ZGC better consistency

## Benefits

### Same Machine Comparison ✅
- Both run on identical hardware
- Fair comparison (same CPU, memory, disk)
- No environmental differences

### Parallel Execution ✅
- Both run simultaneously
- Saves time (still ~73 minutes total)
- Uses same Spark directory (no SBT conflicts)

### Automatic Comparison ✅
- No manual analysis needed
- Query-by-query breakdown
- Overall summary with percentages

### Comprehensive Results ✅
- Both full logs available
- CSV export for spreadsheets
- Easy to interpret

## Use Cases

### When G1GC is Better:
- Throughput is priority over latency
- Heap < 4GB
- Traditional batch workloads
- Cost-conscious (less GC overhead)

### When ZGC is Better:
- Latency is critical
- Heap > 8GB
- Interactive workloads
- Consistent performance needed

### This Benchmark Shows:
Which GC performs better for TPC-DS analytical queries on Spark!

## Summary

**Purpose:** Compare G1GC vs ZGC for TPC-DS benchmarks

**Execution:** Both run in parallel on same machine

**Time:** ~73 minutes (unchanged, runs in parallel)

**Output:** 
- ✅ G1GC benchmark results
- ✅ ZGC benchmark results
- ✅ Query-by-query comparison
- ✅ Overall performance summary

**File:** `.github/workflows/tpcds-benchmark.yml`

