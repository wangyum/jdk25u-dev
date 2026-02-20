# TPC-DS Workflow Update Summary

## What Changed

The `tpcds-benchmark.yml` workflow now accepts manual configuration through workflow_dispatch inputs.

## New Manual Inputs

### 1. baseline_opts
- **Description:** JVM options for baseline G1GC benchmark
- **Default:** `-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch`
- **Example:** `-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:G1HeapRegionSize=4m`

### 2. optimized_opts
- **Description:** JVM options for optimized ZGC benchmark
- **Default:** `-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch`
- **Example:** `-Xms4g -XX:+UseZGC -XX:+AlwaysPreTouch -XX:ZCollectionInterval=10`

### 3. log_level
- **Description:** Spark logging level
- **Options:** `ERROR`, `WARN`, `INFO`
- **Default:** `ERROR`

### 4. query_filter
- **Description:** TPC-DS queries to run (comma-separated)
- **Default:** `q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96`
- **Example:** `q1,q2,q3` for quick testing
- **Example:** `all` for full TPC-DS suite

## How to Use

### Via GitHub Actions UI

1. Go to **Actions** tab
2. Select **TPC-DS Benchmark with JDK 25**
3. Click **Run workflow**
4. Fill in the inputs (or leave defaults)
5. Click **Run workflow**

### Via GitHub CLI

**Quick test (single query):**
```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f query_filter="q3"
```

**Custom JVM options:**
```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms4g -XX:+UseZGC -XX:+AlwaysPreTouch" \
  -f query_filter="q3,q7,q19,q27"
```

**Enable debugging:**
```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f log_level="INFO" \
  -f query_filter="q3"
```

## Workflow Changes

### Step 1: Set benchmark configuration

New step that reads inputs and sets environment variables:

```yaml
- name: Set benchmark configuration
  run: |
    BASELINE_OPTS="${{ github.event.inputs.baseline_opts || '-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch' }}"
    OPTIMIZED_OPTS="${{ github.event.inputs.optimized_opts || '-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch' }}"
    LOG_LEVEL="${{ github.event.inputs.log_level || 'ERROR' }}"
    QUERY_FILTER="${{ github.event.inputs.query_filter || 'q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96' }}"
```

### Updated Benchmark Steps

All benchmark steps now use environment variables:

**Warmup:**
```bash
--conf spark.driver.extraJavaOptions="$BASELINE_OPTS"
--conf spark.log.level="$LOG_LEVEL"
--query-filter "q3"  # Always uses q3 for warmup
```

**G1GC Benchmark (Baseline):**
```bash
--conf spark.driver.extraJavaOptions="$BASELINE_OPTS"
--conf spark.log.level="$LOG_LEVEL"
--query-filter "$QUERY_FILTER"
```

**ZGC Benchmark (Optimized):**
```bash
--conf spark.driver.extraJavaOptions="$OPTIMIZED_OPTS"
--conf spark.log.level="$LOG_LEVEL"
--query-filter "$QUERY_FILTER"
```

## Example Use Cases

### 1. Quick Testing (Single Query)

Test with just q3 to validate workflow:

```bash
gh workflow run tpcds-benchmark.yml --ref spark -f query_filter="q3"
```

**Runtime:** ~2-3 minutes

### 2. Test Specific Queries

Run only fast queries:

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f query_filter="q3,q7,q19,q27,q42,q43"
```

**Runtime:** ~5-10 minutes

### 3. Test Different Heap Sizes

Compare performance with 4GB heap:

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms4g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  -f optimized_opts="-Xms4g -XX:+UseZGC -XX:+AlwaysPreTouch"
```

### 4. Test G1 Region Sizes

Compare different G1 heap region sizes:

```bash
# Run 1: 2MB regions
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:G1HeapRegionSize=2m"

# Run 2: 4MB regions
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f baseline_opts="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch -XX:G1HeapRegionSize=4m"
```

### 5. Debug Run with Verbose Logging

Enable INFO logging to debug issues:

```bash
gh workflow run tpcds-benchmark.yml \
  --ref spark \
  -f log_level="INFO" \
  -f query_filter="q3"
```

## Output Display

The workflow shows configuration at the start:

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

Each benchmark step shows:

```
==========================================
TPC-DS Benchmark - G1GC (Baseline)
==========================================
Configuration: -Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch
Queries: q3,q7,q19,q27,q42,q43,q52,q55,q63,q65,q68,q73,q79,q96
Start: Thu Feb 20 12:34:56 UTC 2026
```

## Benefits

1. **Flexibility:** Test different JVM configurations without editing workflow file
2. **Quick iteration:** Run with single query for fast validation
3. **A/B testing:** Compare different GC settings easily
4. **Debugging:** Enable verbose logging when needed
5. **Safe defaults:** Automatic runs use proven stable configuration

## Files Modified

1. `.github/workflows/tpcds-benchmark.yml`
   - Added workflow_dispatch inputs
   - Added configuration step
   - Updated all benchmark steps to use variables

2. `TPCDS_MANUAL_JVM_OPTIONS.md`
   - Comprehensive documentation
   - Examples and use cases
   - Troubleshooting guide

3. `TPCDS_WORKFLOW_UPDATE_SUMMARY.md`
   - This file - quick reference guide

## Important Notes

- **Warmup always uses q3:** The warmup step always uses q3 query, regardless of query_filter
- **No -Xmx flag:** Don't include `-Xmx` in custom options (Spark sets it automatically)
- **No spaces in query_filter:** Use `q1,q2,q3` not `q1, q2, q3`
- **Keep -Xms:** Always include `-Xms` for AlwaysPreTouch to work properly

## Summary

The TPC-DS workflow is now fully configurable for manual runs while maintaining safe defaults for automatic push triggers. This enables flexible testing and experimentation without workflow file modifications!
