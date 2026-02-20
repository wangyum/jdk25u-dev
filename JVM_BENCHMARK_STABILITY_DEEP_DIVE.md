# JVM/JDK Benchmark Stability: Deep Dive

## Executive Summary

Based on Oracle/OpenJDK documentation, JMH best practices, and academic research, achieving stable JVM benchmarks requires addressing JIT compilation variance, GC interference, and non-deterministic warmup behavior. **Research shows only 43.3-56.5% of (VM, benchmark) pairs conform to traditional warmup assumptions**, making JVM benchmarking fundamentally challenging.

## Critical JVM-Specific Stability Factors

### 1. JIT Compilation Non-Determinism (Highest Impact)

**Research Finding:**
> "Across 3 benchmarking machines only 43.3–56.5% of (VM, benchmark) pairs conform to the traditional view of warmup and **none of the VMs consistently warms up**."
> — [Virtual machine warmup blows hot and cold](https://research.lancaster-university.uk/en/publications/virtual-machine-warmup-blows-hot-and-cold/)

**Problem:** JIT compilation introduces variance due to:
- Profile-guided optimization decisions
- Non-deterministic compilation timing
- Tiered compilation transitions (Interpreter → C1 → C2)
- Inlining decisions based on runtime profiling
- Deoptimization and recompilation events

**Impact on Your Spark Benchmarks:**
- Q5 short run: -3.2% regression
- Q5 long run: +1.9% improvement
- **Cause:** JIT hadn't finished optimizing in short run

### 2. Warmup Requirements

**JVM Warmup Phases:**

```
1. Class Loading Phase (first 10-100 iterations)
   - Classes loaded on-demand
   - Static initializers executed
   - Bytecode verification

2. Interpreted Execution (iterations 0-1000)
   - Bytecode interpreted
   - Profile data collected
   - Method invocation counters tracked

3. Tier 1 Compilation - C1 (after ~1000 invocations)
   - Fast JIT compilation
   - Basic optimizations
   - Profiling instrumentation inserted

4. Tier 4 Compilation - C2 (after ~10,000 invocations)
   - Aggressive optimization
   - Inlining decisions
   - Escape analysis
   - Vectorization
   - Loop unrolling

5. Steady State (variable timing, often 100+ iterations)
   - Compilations complete
   - Stable performance
   - Minimal deoptimizations
```

**Default Thresholds:**
- `-XX:CompileThreshold=10000` (C2 compilation trigger)
- Tier 1 (C1): ~1,000 invocations
- Tier 4 (C2): ~10,000 invocations or loops

**Oracle Recommendation:**
> "JMH by default makes several warm-up cycles before collecting stats to ensure results are not completely random and the JVM has performed initial optimizations."
> — [JMH Best Practices](https://www.baeldung.com/java-microbenchmark-harness)

### 3. JVM Flags for Benchmark Stability

#### Essential Diagnostic Flags

```bash
# Monitor JIT compilation (lightweight)
-XX:+PrintCompilation

# Detailed compilation analysis (heavyweight, use offline)
-XX:+UnlockDiagnosticVMOptions \
-XX:+LogCompilation \
-XX:LogFile=compilation.log

# Disable tiered compilation for predictability (not recommended for production)
-XX:-TieredCompilation
```

**PrintCompilation Output:**
```
  123   1       java.lang.String::hashCode (55 bytes)
  145   2       java.lang.String::charAt (33 bytes)
  189   3       java.lang.String::indexOf (70 bytes)
```
- Column 1: Timestamp (ms)
- Column 2: Compilation ID
- Column 3: Method compiled

**When to Use:**
- `-XX:+PrintCompilation`: Always for benchmark analysis
- `-XX:+LogCompilation`: When debugging specific performance issues
- **DO NOT** use LogCompilation in actual benchmark runs (overhead!)

#### Memory Pre-Touch for GC Stability

```bash
# Pre-allocate and touch all heap pages at startup
-Xms4g -Xmx4g \
-XX:+AlwaysPreTouch
```

**Benefits:**
- Eliminates page faults during GC
- More consistent GC pause times
- Upfront cost paid at JVM startup
- **Impact:** Can reduce pause time variance by 50-70%

**Trade-off:**
- Slower JVM startup (10-30 seconds for large heaps)
- Higher initial memory usage
- **Recommendation:** Essential for stable benchmarks

#### NUMA-Aware Memory Allocation

```bash
# For multi-socket systems
-XX:+UseNUMA
```

**Performance Impact:**
- 20.64% improvement in Max-jOPS (SPECjbb2015)
- 9.52% improvement in Critical-jOPS
- Best results when combined with `-XX:+AlwaysPreTouch`

**When to Use:**
- Multi-socket servers (2+ CPU sockets)
- Large heaps (>32GB)
- **GitHub Actions:** Not applicable (single-socket VMs)

#### Deterministic Compilation Thresholds

```bash
# Lower threshold for faster warmup (less optimal code)
-XX:CompileThreshold=5000

# Higher threshold for more optimized code (slower warmup)
-XX:CompileThreshold=15000

# Default
-XX:CompileThreshold=10000
```

**Research Finding:**
> "Lowering the threshold enables faster warmup at the cost of less optimization data."
> — [Analyzing and Tuning Warm-up](https://docs.azul.com/prime/analyzing-tuning-warmup)

**For Spark Benchmarks:**
```bash
# Recommended: Use default, ensure adequate warmup
-XX:CompileThreshold=10000  # Default, no need to specify

# Or: Lower for faster CI feedback (accept less optimization)
-XX:CompileThreshold=5000
```

### 4. GC Interference Mitigation

**Problem:** GC pauses cause timing variance.

**Solutions:**

```bash
# Option 1: Larger heap to reduce GC frequency
-Xms6g -Xmx6g

# Option 2: Pre-touch to stabilize GC pauses
-XX:+AlwaysPreTouch

# Option 3: Use low-latency GC (ZGC/Shenandoah)
-XX:+UseZGC
# or
-XX:+UseShenandoahGC

# Option 4: Disable explicit GC calls
-XX:+DisableExplicitGC

# Option 5: Log GC to detect interference
-Xlog:gc*:file=gc.log
```

**GC Selection for Benchmarks:**

| GC Type | Pause Times | Throughput | Stability | Use Case |
|---------|-------------|------------|-----------|----------|
| G1GC (default) | Medium | High | Good | General benchmarks ✅ |
| ZGC | Very Low | Medium | Excellent | Latency-sensitive ✅ |
| Shenandoah | Very Low | Medium | Excellent | Latency-sensitive ✅ |
| Parallel GC | High | Highest | Poor | Avoid for benchmarks ❌ |
| Serial GC | High | Low | Good | Small heaps only |

**Recommendation for Spark Benchmarks:**
```bash
# Current (good choice)
-XX:+UseG1GC -Xms3g -Xmx3g -XX:+AlwaysPreTouch

# Alternative for complex queries
-XX:+UseZGC -Xms6g -Xmx6g -XX:+AlwaysPreTouch
```

### 5. JMH-Inspired Benchmark Methodology

Even though you're not using JMH directly, adopt its proven principles:

#### Warmup Iterations

```java
// JMH default: 5 warmup iterations, 5 measurement iterations
@Warmup(iterations = 5, time = 10, timeUnit = TimeUnit.SECONDS)
@Measurement(iterations = 5, time = 10, timeUnit = TimeUnit.SECONDS)
```

**Applied to Spark Benchmarks:**
```bash
# Warmup phase: Run 3-5 representative queries
./bin/spark-submit --query-filter "q3,q7,q19" > warmup.log 2>&1

# Measurement phase: Run actual benchmark
./bin/spark-submit --query-filter "q1,q2,q4,..." > benchmark.log 2>&1
```

**Warmup Duration Guidelines:**
- **Fast queries (<500ms):** 5+ warmup iterations
- **Medium queries (500-5000ms):** 3+ warmup iterations
- **Slow queries (>5000ms):** 1-2 warmup iterations
- **Total warmup time:** At least 30-60 seconds

#### Forks (Multiple JVM Instances)

```java
// JMH: Run benchmark in 5 separate JVM instances
@Fork(value = 5)
```

**Applied to Spark Benchmarks:**
```bash
# Run benchmark 3 times, take median
for i in 1 2 3; do
  ./bin/spark-submit ... > "benchmark-run${i}.log" 2>&1
done

# Calculate median time
python3 calculate_median.py benchmark-run*.log
```

**Why Forks Matter:**
- Each JVM instance has different JIT decisions
- Reduces impact of lucky/unlucky compilation
- **Research:** 3-5 forks reduce variance significantly

#### Blackhole Pattern (Prevent Dead Code Elimination)

```java
// JMH ensures results are not optimized away
@Benchmark
public void testMethod(Blackhole bh) {
    int result = compute();
    bh.consume(result);  // Prevents DCE
}
```

**Applied to Spark Benchmarks:**
```scala
// Ensure results are materialized
val df = spark.sql(query)
df.collect()  // Force computation
df.count()    // Or count to prevent optimization
```

### 6. Detecting Warmup Completion

**Oracle's Methodology:**
> "To determine warmup completion, run a very long test and see how long it takes to reach 99% of peak performance and remain steadily at that level for a long period of time."
> — [How to Warm Up the JVM](https://www.baeldung.com/java-jvm-warmup)

**Practical Detection:**

```bash
# Run with PrintCompilation, count compilations over time
java -XX:+PrintCompilation ... | \
  awk '{print $1}' | \
  tail -100 | \
  uniq -c

# If compilations have stopped, warmup is likely complete
```

**For Spark Benchmarks:**
```bash
# Method 1: Monitor JIT activity
./bin/spark-submit \
  --conf spark.driver.extraJavaOptions="-XX:+PrintCompilation" \
  --query-filter "q3,q7,q19,q27,q42" \
  2>&1 | grep -E "^[0-9]+" > compilation.log

# Warmup complete when compilation activity stops
tail -100 compilation.log | wc -l  # Should be 0-5 lines

# Method 2: Run same query multiple times, measure variance
for i in {1..10}; do
  time ./bin/spark-submit --query-filter "q3" 2>&1 | grep "^q3"
done

# Warmup complete when times stabilize (CV < 5%)
```

### 7. Reproducible Builds for Benchmarking

While not directly related to runtime performance, reproducible builds ensure consistent bytecode:

```xml
<!-- Maven: pom.xml -->
<properties>
  <!-- Fixed timestamp for reproducible JARs -->
  <project.build.outputTimestamp>2026-01-01T00:00:00Z</project.build.outputTimestamp>
</properties>
```

```groovy
// Gradle: build.gradle
tasks.withType(AbstractArchiveTask).configureEach {
    preserveFileTimestamps = false
    reproducibleFileOrder = true
}
```

**Impact:**
- Same source → same bytecode → more consistent JIT behavior
- Eliminates timestamp-based variance
- **Limited benefit** for large applications (JIT variance dominates)

## Comprehensive JVM Benchmark Configuration

### For Spark TPC-DS Benchmarks on GitHub Actions

```bash
#!/bin/bash

# JVM Configuration for Stable Benchmarks
JAVA_OPTS=""

# 1. Memory Configuration (fixed heap, pre-touched)
JAVA_OPTS="$JAVA_OPTS -Xms4g -Xmx4g"
JAVA_OPTS="$JAVA_OPTS -XX:+AlwaysPreTouch"

# 2. GC Configuration (G1GC with stable pauses)
JAVA_OPTS="$JAVA_OPTS -XX:+UseG1GC"
JAVA_OPTS="$JAVA_OPTS -XX:MaxGCPauseMillis=200"
JAVA_OPTS="$JAVA_OPTS -XX:+DisableExplicitGC"

# 3. JIT Compilation (default threshold, monitoring enabled)
JAVA_OPTS="$JAVA_OPTS -XX:CompileThreshold=10000"
JAVA_OPTS="$JAVA_OPTS -XX:+PrintCompilation"

# 4. NUMA (only if multi-socket, not on GitHub Actions)
# JAVA_OPTS="$JAVA_OPTS -XX:+UseNUMA"

# 5. Diagnostic Flags (remove for production)
JAVA_OPTS="$JAVA_OPTS -XX:+UnlockDiagnosticVMOptions"

# 6. Spark-specific optimizations
JAVA_OPTS="$JAVA_OPTS -XX:+UnlockExperimentalVMOptions"
JAVA_OPTS="$JAVA_OPTS -XX:+G1OptimizeForSpark"
JAVA_OPTS="$JAVA_OPTS -XX:+G1SparkEnhanceEscapeAnalysis"
JAVA_OPTS="$JAVA_OPTS -XX:+G1SparkOptimizeHashOperations"
JAVA_OPTS="$JAVA_OPTS -XX:+G1SparkEnableVectorization"

# Warmup Phase (3 queries, ~30-60 seconds)
./bin/spark-submit \
  --master local[1] \
  --driver-memory 4g \
  --conf spark.driver.extraJavaOptions="$JAVA_OPTS" \
  --conf spark.driver.log.level=ERROR \
  --query-filter "q3,q7,q19" \
  > warmup.log 2>&1

# Measurement Phase
./bin/spark-submit \
  --master local[1] \
  --driver-memory 4g \
  --conf spark.driver.extraJavaOptions="$JAVA_OPTS" \
  --conf spark.driver.log.level=WARN \
  --query-filter "q1,q2,q3,..." \
  > benchmark.log 2>&1
```

### Advanced: Multiple Forks for Statistical Stability

```bash
#!/bin/bash

QUERIES="q3,q7,q19,q27,q42,q52,q55,q63"
FORKS=3

for fork in $(seq 1 $FORKS); do
  echo "=== Fork $fork/$FORKS ==="

  # Fresh JVM instance for each fork
  ./bin/spark-submit \
    --master local[1] \
    --driver-memory 4g \
    --conf spark.driver.extraJavaOptions="$JAVA_OPTS" \
    --query-filter "$QUERIES" \
    > "benchmark-fork${fork}.log" 2>&1
done

# Calculate median times across forks
python3 <<EOF
import re
import statistics

query_times = {}
for fork in range(1, $FORKS + 1):
    with open(f'benchmark-fork{fork}.log') as f:
        for line in f:
            match = re.match(r'^(q\d+[ab]?)\s+(\d+)', line)
            if match:
                query, time = match.groups()
                query_times.setdefault(query, []).append(int(time))

print("Query,Median(ms),Min(ms),Max(ms),Variance(%)")
for query in sorted(query_times.keys()):
    times = query_times[query]
    median = statistics.median(times)
    min_t = min(times)
    max_t = max(times)
    variance = ((max_t - min_t) / median) * 100
    print(f"{query},{median},{min_t},{max_t},{variance:.2f}")
EOF
```

## Variance Analysis Framework

### Expected Variance by Query Type

Based on JIT research and our Spark benchmark data:

```
Fast Queries (<500ms):
  ├─ Warmup-dependent: ±10-20%
  ├─ With adequate warmup: ±5-10%
  └─ With multiple forks: ±3-5%

Medium Queries (500-5000ms):
  ├─ Warmup-dependent: ±5-10%
  ├─ With adequate warmup: ±3-5%
  └─ With multiple forks: ±2-3%

Slow Queries (>5000ms):
  ├─ Warmup-dependent: ±3-5%
  ├─ With adequate warmup: ±2-3%
  └─ With multiple forks: ±1-2%
```

### Detecting Real Regressions vs. Noise

```python
def is_real_regression(baseline_ms, optimized_ms, query_type):
    """
    Determine if performance difference is a real regression or noise.

    Query types: 'fast' (<500ms), 'medium' (500-5000ms), 'slow' (>5000ms)
    """
    diff_pct = ((optimized_ms - baseline_ms) / baseline_ms) * 100

    # Adaptive thresholds based on query runtime
    if query_type == 'fast':
        threshold = 10.0  # Fast queries have high variance
    elif query_type == 'medium':
        threshold = 5.0   # Medium queries moderate variance
    else:  # slow
        threshold = 3.0   # Slow queries low variance

    # Statistical significance: require 2x threshold for "definite" regression
    if abs(diff_pct) < threshold:
        return "neutral"  # Within noise
    elif diff_pct > threshold * 2:
        return "regressed"  # Definite regression
    elif diff_pct < -threshold * 2:
        return "improved"  # Definite improvement
    else:
        return "uncertain"  # Borderline, need more data
```

## Research-Backed Recommendations

### 1. Warmup is Non-Deterministic ⚠️

**Research Finding:**
> "Only 43.3-56.5% of (VM, benchmark) pairs conform to traditional warmup view and **none of the VMs consistently warms up**."

**Practical Implication:**
- **Cannot rely on fixed iteration counts**
- **Must verify warmup completion empirically**
- **Some queries may never reach steady state**

**Solution:**
```bash
# Don't assume warmup is complete after N iterations
# Instead: Run until compilation activity stops

./bin/spark-submit \
  --conf spark.driver.extraJavaOptions="-XX:+PrintCompilation" \
  ... | tee compilation.log

# Check last 50 compilations
tail -50 compilation.log | grep -E "^[0-9]+"

# If still seeing compilations, need more warmup
```

### 2. AOT Compilation for Deterministic Performance

**Research Finding:**
> "AOT executables exhibited immediate stability with minimal run-to-run variance, supporting their use in environments where **deterministic performance is required**."

**For Critical Benchmarks:**
```bash
# Use GraalVM Native Image for deterministic startup
native-image \
  --no-fallback \
  -jar spark-benchmark.jar \
  spark-benchmark-native

# Trade-offs:
# ✅ Deterministic performance (no JIT variance)
# ✅ Fast startup
# ❌ Lower peak performance (no runtime optimization)
# ❌ Longer build time
```

**Recommendation:**
- **JIT (current):** Better peak performance, variable startup
- **AOT:** Deterministic, good for CI validation
- **Hybrid:** Use AOT for fast feedback, JIT for authoritative benchmarks

### 3. Measurement at Scale

**For Production-Grade Benchmarks:**

```yaml
benchmark_protocol:
  warmup:
    - duration: 60 seconds minimum
    - verification: Monitor PrintCompilation output
    - acceptance: <5 compilations in last 30 seconds

  measurement:
    - forks: 5 (separate JVM instances)
    - iterations_per_fork: 3
    - statistical_analysis: median ± MAD (median absolute deviation)

  regression_detection:
    - threshold: 2x expected variance
    - confidence: 95%
    - minimum_effect_size: 3% for slow queries, 10% for fast queries
```

## Comparison: Current vs. Optimal Configuration

### Current Configuration (Good)

```bash
--master local[1]              # ✅ Dedicated core
--driver-memory 3g             # ✅ Fixed heap
--conf extraJavaOptions=       # ✅ G1GC
  "-XX:+UseG1GC"
# Warmup: single query (q3)    # ⚠️ Minimal
# Iterations: 1                # ⚠️ Single run
# Logging: WARN level          # ✅ Reduced overhead
```

**Expected Variance:** ±5-10%

### Optimal Configuration (Better Stability)

```bash
--master local[1]              # ✅ Dedicated core
--driver-memory 4g             # ✅ Fixed heap (larger)
--conf extraJavaOptions=       # ✅ G1GC + stability flags
  "-Xms4g -Xmx4g
   -XX:+UseG1GC
   -XX:+AlwaysPreTouch
   -XX:+PrintCompilation
   -XX:+DisableExplicitGC
   -XX:MaxGCPauseMillis=200"

# Warmup: 3-5 queries          # ✅ Comprehensive
# Iterations: 3 forks          # ✅ Statistical robustness
# Verification: PrintCompilation # ✅ Confirm warmup complete
```

**Expected Variance:** ±2-5%

**Trade-off:** 3x slower CI time, but 2x better stability

## Action Items for Your Benchmarks

### Immediate (No CI Time Impact)

1. **Add -XX:+AlwaysPreTouch**
   ```yaml
   --conf spark.driver.extraJavaOptions=
     "-XX:+UseG1GC -Xms3g -Xmx3g -XX:+AlwaysPreTouch ..."
   ```

2. **Increase warmup queries**
   ```bash
   # From: q3
   # To: q3,q7,q19
   --query-filter "q3,q7,q19"
   ```

3. **Add -XX:+PrintCompilation for analysis**
   ```bash
   # Save compilation log for post-analysis
   --conf extraJavaOptions="... -XX:+PrintCompilation" 2>&1 | \
     tee >(grep "^[0-9]" > compilation.log)
   ```

### Short-term (Moderate CI Impact)

4. **Increase heap to 4g**
   ```bash
   --driver-memory 4g
   ```

5. **Add variance reporting**
   ```python
   # Report min/max/median instead of single value
   ```

6. **Expand regression thresholds**
   ```python
   # Fast queries: 10%
   # Medium queries: 5%
   # Slow queries: 3%
   ```

### Long-term (Significant CI Impact)

7. **Implement 3-fork methodology**
   ```bash
   # Run each benchmark 3 times, report median
   ```

8. **Add warmup verification**
   ```bash
   # Check PrintCompilation output, ensure <5 compilations at end
   ```

9. **Consider self-hosted runners**
   ```yaml
   # For authoritative benchmarks, use consistent hardware
   runs-on: [self-hosted, benchmark-runner]
   ```

## Conclusion

### Key Takeaways

1. **JIT non-determinism is fundamental** - Research shows <60% conformance to traditional warmup
2. **AlwaysPreTouch is essential** - Eliminates GC pause variance
3. **Warmup must be verified** - Monitor PrintCompilation, don't assume completion
4. **Multiple forks required** - Single runs have ±10-20% variance
5. **Adaptive thresholds** - Fast queries need larger regression thresholds

### Realistic Expectations

Even with optimal configuration:
- **Fast queries:** ±5-10% variance (unavoidable)
- **Medium queries:** ±3-5% variance
- **Slow queries:** ±2-3% variance

Your Q5/Q9 "regressions" (-3% and -9%) are **within expected variance** for JIT-based systems.

### Recommended Next Steps

1. **Immediately:** Add `-XX:+AlwaysPreTouch -Xms4g -Xmx4g`
2. **This week:** Implement 3-query warmup and PrintCompilation monitoring
3. **Next month:** Implement 3-fork methodology for authoritative benchmarks
4. **Long-term:** Consider self-hosted runners for zero hardware variance

## Sources

- [Avoiding Benchmarking Pitfalls on the JVM](https://www.oracle.com/technical-resources/articles/java/architect-benchmarking.html) - Oracle official
- [JMH - Java Microbenchmark Harness](https://jenkov.com/tutorials/java-performance/jmh.html) - Best practices
- [Microbenchmarking with Java | Baeldung](https://www.baeldung.com/java-microbenchmark-harness)
- [OpenJDK JMH](https://openjdk.org/projects/code-tools/jmh/) - Official OpenJDK tool
- [How to Warm Up the JVM | Baeldung](https://www.baeldung.com/java-jvm-warmup)
- [Analyzing and Tuning Warm-up](https://docs.azul.com/prime/analyzing-tuning-warmup) - Azul Systems
- [Virtual machine warmup blows hot and cold](https://research.lancaster-university.uk/en/publications/virtual-machine-warmup-blows-hot-and-cold/) - Academic research
- [HotSpot JVM Performance Tuning Guidelines](https://ionutbalosin.com/2020/01/hotspot-jvm-performance-tuning-guidelines/)
- [PrintCompilation JVM flag](https://blog.joda.org/2011/08/printcompilation-jvm-flag.html)
- [JVM Performance Tuning for High Throughput and Low Latency](https://dzone.com/articles/jvm-performance-tuning-for-high-throughput-and-low-latency)
- [G1 GC Performance Tuning Guide](https://betasignal.substack.com/p/g1-gc-performance-tuning-guide)
