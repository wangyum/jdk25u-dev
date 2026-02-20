# TPC-DS Benchmark Manual JVM Options

## Overview

The `tpcds-benchmark.yml` workflow now supports manual configuration of JVM options through GitHub Actions workflow_dispatch inputs.

## Default Configuration

When triggered by push (automatic):

- **Baseline (G1GC):** `-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch`
- **Optimized (ZGC):** `-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch`
- **Log Level:** `ERROR`
- **Query Filter:** `q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96`

## Manual Trigger

### Via GitHub Actions UI

1. Go to: **Actions** → **TPC-DS Benchmark with JDK 25** → **Run workflow**
2. Select branch (usually `spark` or `main`)
3. Configure options:

#### Input Options

**Baseline JVM options (G1GC):**
- Input field: `baseline_opts`
- Description: JVM options for baseline G1GC benchmark
- Default: `-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch`
- Example custom: `-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:G1HeapRegionSize=4m`

**Optimized JVM options (ZGC):**
- Input field: `optimized_opts`
- Description: JVM options for optimized ZGC benchmark
- Default: `-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch`
- Example custom: `-Xms4g -XX:+UseZGC -XX:+AlwaysPreTouch -XX:ZCollectionInterval=5`

**Spark log level:**
- Input field: `log_level`
- Description: Spark logging level
- Options: `ERROR`, `WARN`, `INFO`
- Default: `ERROR`

**Query filter:**
- Input field: `query_filter`
- Description: TPC-DS queries to run (comma-separated, no spaces)
- Default: `q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96`
- Example custom: `q1,q2,q3` (run only first 3 queries)
- Example all: Use `all` to run all TPC-DS queries

### Via GitHub CLI

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch" \
  -f log_level="ERROR" \
  -f query_filter="q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96"
```

### Example Custom Configurations

#### Quick Test with Single Query

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch" \
  -f query_filter="q3"
```

#### Run Specific Queries

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f query_filter="q1,q2,q3,q4,q5"
```

#### Test Different Heap Sizes

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms4g -XX:+UseZGC -XX:+AlwaysPreTouch"
```

#### Test G1 Region Sizes

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:G1HeapRegionSize=2m" \
  -f optimized_opts="-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch"
```

#### Test ZGC Collection Interval

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch -XX:ZCollectionInterval=10"
```

#### Enable Verbose Logging for Debugging

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch" \
  -f log_level="INFO"
```

## Workflow Structure

The workflow runs 3 benchmarks:

1. **Warmup:** Uses `baseline_opts` (G1GC) for JIT warmup
2. **G1GC Benchmark (Baseline):** Uses `baseline_opts`
3. **ZGC Benchmark (Optimized):** Uses `optimized_opts`

Each benchmark outputs results to separate files:
- Warmup: (not saved, console output only)
- G1GC: `benchmark-g1gc.log`
- ZGC: `benchmark-zgc.log`

## Output Example

When the workflow runs, you'll see:

```
==========================================
Benchmark Configuration
==========================================
Baseline opts (G1GC):  -Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch
Optimized opts (ZGC):  -Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch
Log level:             ERROR
Query filter:          q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96
==========================================
```

Then each benchmark step shows the configuration being used:

```
==========================================
TPC-DS Benchmark - G1GC (Baseline)
==========================================
Configuration: -Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch
Queries: q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96
Start: Thu Feb 20 12:34:56 UTC 2026
```

## Important Notes

### 1. Keep -Xms Flag

Always include `-Xms3g` (or your chosen heap size) to ensure:
- AlwaysPreTouch pre-touches the full heap
- No heap resizing during benchmark
- Consistent performance

### 2. Don't Include -Xmx

Spark automatically sets `-Xmx` from `--driver-memory 3g`. Including `-Xmx` in extraJavaOptions will cause an error:

```
Error: Not allowed to specify max heap(Xmx) memory settings through java options
```

### 3. Log Level

- `ERROR`: Minimal logging, cleanest benchmark output (recommended)
- `WARN`: Shows warnings but suppresses INFO
- `INFO`: Full logging (useful for debugging)

### 4. Query Filter Format

The query filter must be:
- Comma-separated with NO spaces: `q1,q2,q3` ✅
- NOT with spaces: `q1, q2, q3` ❌
- Use lowercase `q` prefix: `q3` ✅ (not `Q3` ❌)
- Valid query names from TPC-DS benchmark suite

Available queries (99 total):
- `q1` through `q99` - Standard TPC-DS queries
- Use `all` to run all queries (takes 6+ hours)

Common query subsets:
- **Fast queries:** `q3,q7,q19,q27,q42,q43` (< 1 minute total)
- **Medium queries:** `q52,q55,q63,q65,q68,q73,q79,q96` (1-3 minutes total)
- **Default (balanced):** `q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96`
- **Full suite:** `all` (6+ hours)

### 5. Custom GC Flags

You can add any JVM flags to test different configurations:

**G1GC flags:**
- `-XX:G1HeapRegionSize=<size>` - Region size (1m, 2m, 4m, etc.)
- `-XX:MaxGCPauseMillis=<ms>` - Target max pause time
- `-XX:G1ReservePercent=<percent>` - Reserve memory percent
- `-XX:InitiatingHeapOccupancyPercent=<percent>` - When to start concurrent cycle

**ZGC flags:**
- `-XX:ZCollectionInterval=<seconds>` - Minimum time between GC cycles
- `-XX:ZFragmentationLimit=<percent>` - Fragmentation threshold
- `-XX:ZAllocationSpikeTolerance=<value>` - Spike tolerance

## Verification

After running with custom options, check the workflow logs to verify:

1. Options are correctly displayed in the "JVM Options Configuration" section
2. Each benchmark step shows the correct configuration
3. Spark-submit command uses the correct extraJavaOptions
4. No errors about prohibited options (-Xmx, etc.)

## Comparison Results

The workflow automatically generates comparison between G1GC and ZGC using the Python script. Results include:

- Performance comparison (time improvements)
- Query-by-query analysis
- Statistical summary

Artifacts are uploaded with name: `tpcds-gc-comparison-<git-sha>`

## Example Workflow Run

**Scenario:** Test if larger heap (4GB) improves performance

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms4g -XX:+UseZGC -XX:+AlwaysPreTouch"
```

**Note:** You'll need to also change `--driver-memory` to 4g if testing 4GB heap, but that requires editing the workflow file.

## Tips

1. **Start with defaults** to establish baseline performance
2. **Change one variable at a time** to isolate effects
3. **Run multiple times** to account for variance
4. **Use ERROR log level** for production benchmarks
5. **Use INFO log level** only when debugging issues

## Troubleshooting

**Problem:** Workflow fails with "Not allowed to specify max heap(Xmx)"

**Solution:** Remove `-Xmx` from your custom options. Spark sets this automatically.

---

**Problem:** No performance difference seen

**Solution:**
- Ensure options are actually different
- Check workflow logs to verify correct options used
- Some flags may not affect this specific workload

---

**Problem:** Benchmark crashes with OutOfMemoryError

**Solution:**
- Your `-Xms` value might be too large for the runner
- GitHub Actions runners have limited memory
- Try smaller heap size or use self-hosted runner

## Advanced Usage

### Testing Custom JDK Builds

If you're testing custom JDK features, you can enable specific experimental flags:

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch -XX:+UnlockExperimentalVMOptions -XX:+MyCustomFeature"
```

### Comparing Two G1GC Configurations

Since both benchmarks accept custom options, you could compare two G1GC configurations:

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:G1HeapRegionSize=2m" \
  -f optimized_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:G1HeapRegionSize=4m"
```

The comparison script will still work, comparing "baseline" vs "optimized" configurations.

## Summary

The TPC-DS benchmark workflow now provides full flexibility to test different JVM configurations while maintaining safe defaults. This enables:

- A/B testing of GC algorithms
- Performance tuning experiments
- Custom JDK feature validation
- Debugging with different log levels

All through the GitHub Actions UI or CLI, without editing workflow files!
