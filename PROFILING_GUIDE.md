# Profiling Guide: When JVM Optimizations Help

## Problem: Not All Workloads Benefit from JVM Optimizations

Your stack traces show **I/O-bound workloads** where JVM optimizations have minimal impact:
- ❌ LZ4 native compression (shuffle writes)
- ❌ Parquet file writing (disk I/O)
- ❌ Network I/O (shuffle reads)

## Workload Classification

### CPU-Bound (JVM optimizations HELP) ✅

**Characteristics:**
- High CPU utilization (>70%)
- Low I/O wait time
- Frequent GC pauses
- Heavy object allocation

**Example operations:**
- In-memory aggregations
- Complex transformations (map, flatMap, filter)
- String parsing and manipulation
- Hash joins on small datasets (broadcast joins)
- UDF-heavy workloads

**JVM optimization impact:** 20-60% improvement

### I/O-Bound (JVM optimizations DON'T HELP) ❌

**Characteristics:**
- Low CPU utilization (<50%)
- High I/O wait time
- Reading/writing large files
- Shuffle-heavy operations

**Example operations:**
- Reading Parquet/ORC files
- Writing results to storage
- Shuffle writes (sort, groupBy with many partitions)
- Network-heavy operations

**JVM optimization impact:** <5% improvement

## How to Identify Your Bottleneck

### Method 1: Check Spark UI

1. Open Spark UI (usually http://localhost:4040)
2. Go to "Stages" tab
3. Look at task metrics:

```
If "Shuffle Write Time" > 50% of task time:
  → I/O bound, optimize shuffle/storage

If "GC Time" > 20% of task time:
  → GC bound, JVM optimizations will help

If "CPU Time" is high but GC is low:
  → CPU bound but efficient GC, optimize algorithms
```

### Method 2: Add GC Logging

```bash
spark-submit \
  --conf spark.executor.extraJavaOptions="\
    -Xlog:gc*:file=/tmp/gc-%p.log:time,level,tags \
    -Xlog:safepoint:file=/tmp/safepoint-%p.log" \
  your-app.jar
```

**Analyze logs:**
```bash
# Count GC pauses
grep "Pause" /tmp/gc-*.log | wc -l

# Total GC time
grep "Pause" /tmp/gc-*.log | \
  awk '{sum+=$NF} END {print "Total GC time:", sum, "ms"}'

# Average pause time
grep "Pause" /tmp/gc-*.log | \
  awk '{sum+=$NF; count++} END {print "Avg pause:", sum/count, "ms"}'
```

**Interpretation:**
- GC time < 5%: Not GC bound, optimize I/O or algorithms
- GC time 5-20%: Moderate GC impact, some optimization benefit
- GC time > 20%: GC bound, **your optimizations will help significantly**

### Method 3: Async Profiler

```bash
# Download async-profiler
wget https://github.com/async-profiler/async-profiler/releases/latest/download/async-profiler-3.0-linux-x64.tar.gz
tar xzf async-profiler-3.0-linux-x64.tar.gz

# Run Spark with profiling
spark-submit \
  --conf spark.executor.extraJavaOptions="\
    -XX:+UnlockDiagnosticVMOptions \
    -XX:+DebugNonSafepoints \
    -agentpath:/path/to/async-profiler/lib/libasyncProfiler.so=start,event=cpu,file=/tmp/profile.html" \
  your-app.jar
```

**Look for in flame graph:**
- Tall stacks in JVM code (allocation, GC) → JVM optimizations help
- Tall stacks in native code (LZ4, Parquet) → Optimize I/O
- Tall stacks in `java.io.*` → I/O bound

## Optimization Decision Tree

```
Is GC time > 20%?
├─ YES → Enable JVM optimizations (escape analysis, TLAB, etc.)
└─ NO → Is I/O wait high?
    ├─ YES → Optimize storage/shuffle
    │   ├─ Disable compression: spark.shuffle.compress=false
    │   ├─ Faster codec: spark.io.compression.codec=snappy
    │   ├─ SSD storage: spark.local.dir=/fast/storage
    │   └─ Reduce shuffle: spark.sql.adaptive.enabled=true
    └─ NO → Is CPU utilization > 70%?
        ├─ YES → Optimize algorithms (better queries, caching)
        └─ NO → Check network/cluster resource contention
```

## Test CPU-Bound Workload

Run the provided test to verify optimizations work:

```bash
./test-jvm-optimizations.sh
```

This test uses **CPU-bound operations** (heavy allocation, string ops, hash joins) where your optimizations SHOULD show 20-40% improvement.

If you see improvement in the test but not in production:
→ Your production workload is I/O bound, not CPU bound

## Queries That Benefit Most

### ✅ GOOD: CPU-bound queries

```sql
-- High allocation from temporary objects
SELECT
  COUNT(DISTINCT user_id),
  AVG(value * 2 + offset - 10) as computed
FROM large_table
GROUP BY category;

-- String-heavy operations
SELECT
  CONCAT(first_name, ' ', last_name) as full_name,
  UPPER(SUBSTRING(email, 1, 10)) as email_prefix
FROM users;

-- Broadcast hash join (in memory)
SELECT /*+ BROADCAST(small_table) */ *
FROM large_table
JOIN small_table ON large_table.id = small_table.id;
```

### ❌ BAD: I/O-bound queries

```sql
-- Writes to storage (Parquet I/O)
CREATE TABLE output AS
SELECT * FROM huge_table WHERE condition;

-- Large shuffle (network + disk I/O)
SELECT category, COUNT(*)
FROM massive_table
GROUP BY category  -- If many categories → large shuffle
```

## Recommendations for Your Workload

Based on your stack traces (LZ4 compression, Parquet writing), try:

1. **Reduce I/O overhead:**
   ```scala
   spark.conf.set("spark.shuffle.compress", "false")
   spark.conf.set("spark.sql.parquet.compression.codec", "snappy")
   spark.conf.set("spark.local.dir", "/path/to/ssd")
   ```

2. **Reduce shuffle size:**
   ```scala
   spark.conf.set("spark.sql.adaptive.enabled", "true")
   spark.conf.set("spark.sql.adaptive.coalescePartitions.enabled", "true")
   spark.conf.set("spark.sql.autoBroadcastJoinThreshold", "100MB")
   ```

3. **Keep JVM optimizations** - they help on different queries even if not this one

4. **Profile with GC logging** to find queries where GC is actually the bottleneck

## Summary

| Bottleneck | Your Stack Traces | Optimization |
|------------|-------------------|--------------|
| LZ4 compression | ✅ Seen | Disable shuffle compression |
| Parquet I/O | ✅ Seen | Use snappy, larger row groups |
| GC pauses | ❓ Unknown | Enable JVM optimizations |
| CPU computation | ❓ Unknown | Algorithm/query optimization |

**Next step:** Run `./test-jvm-optimizations.sh` to verify your JDK optimizations work, then profile production workload to find real bottleneck.
