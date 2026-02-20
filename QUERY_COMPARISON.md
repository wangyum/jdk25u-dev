# Query-by-Query Performance Comparison

## What Changed

The benchmark analysis now does **detailed query-by-query comparison** with automatic improvement calculation.

## Before (Basic)

```
Results Summary:
----------------
BASELINE output:
[last 20 lines with "time" keyword]

OPTIMIZED output:
[last 20 lines with "time" keyword]
```

❌ No actual comparison
❌ No improvement percentages
❌ Manual review required

## After (Detailed)

```
==============================================================
QUERY-BY-QUERY COMPARISON
==============================================================

Query      Baseline (ms)   Optimized (ms)  Improvement
------------------------------------------------------------
q3         1234.5          987.3           ✅ +20.0%
q7         2345.6          2100.4          ✅ +10.5%
q19        3456.7          3200.1          ✅ +7.4%
q27        4567.8          4800.2          ❌ -5.1%  (regression)
q42        5678.9          4500.0          ✅ +20.8%
...
------------------------------------------------------------
TOTAL      45678.9         40123.4         ✅ +12.2%

==============================================================
Overall Performance: +12.2% improvement
==============================================================
```

✅ Automatic comparison
✅ Improvement percentages
✅ Highlights regressions
✅ Overall summary

## How It Works

### 1. Python Analysis Script

Creates `compare_results.py` that:
- Parses both benchmark logs
- Extracts query execution times
- Calculates improvement percentages
- Detects regressions (>5% slower)
- Generates summary table

### 2. Pattern Matching

Supports multiple benchmark output formats:

**Pattern 1: Direct format**
```
q3: 1234 ms
q7: 5678 ms
```

**Pattern 2: Table format**
```
| q3 | 1234 |
| q7 | 5678 |
```

**Pattern 3: Verbose format**
```
Query q3 took 1234 ms
Query q7 took 5678 seconds
```

### 3. Output Files

**comparison_output.txt** - Human-readable comparison
```
Query      Baseline (ms)   Optimized (ms)  Improvement
q3         1234.5          987.3           ✅ +20.0%
TOTAL      45678.9         40123.4         +12.2%
```

**comparison_results.txt** - CSV format for further analysis
```csv
Query,Baseline(ms),Optimized(ms),Improvement(%)
q3,1234.5,987.3,20.0
q7,2345.6,2100.4,10.5
TOTAL,45678.9,40123.4,12.2
```

### 4. Markdown Summary

Automatically generates comparison table in `benchmark-summary.md`:

```markdown
## Performance Improvement

### Query-by-Query Comparison
[formatted output]

### Detailed Results Table
| Query | Baseline (ms) | Optimized (ms) | Improvement |
|-------|---------------|----------------|-------------|
| q3    | 1234.5        | 987.3          | 20.0%       |
| q7    | 2345.6        | 2100.4         | 10.5%       |
| **TOTAL** | **45678.9** | **40123.4** | **12.2%** |
```

## Regression Detection

Queries that perform worse are flagged:

```
q27        4567.8          4800.2          ❌ -5.1%  (regression)
```

This helps identify:
- Which optimizations hurt specific queries
- Unexpected performance degradation
- Need for query-specific tuning

## Example Output

### Successful Comparison
```
==============================================================
QUERY-BY-QUERY COMPARISON
==============================================================

Query      Baseline (ms)   Optimized (ms)  Improvement
------------------------------------------------------------
q3         2345.1          1876.4          ✅ +20.0%
q7         1234.5          1111.0          ✅ +10.0%
q19        3456.7          3250.8          ✅ +6.0%
q27        987.3           1037.6          ❌ -5.1%
q42        4567.8          3654.2          ✅ +20.0%
q43        2345.6          2110.0          ✅ +10.0%
q52        1876.4          1688.7          ✅ +10.0%
q55        3210.9          2889.8          ✅ +10.0%
q63        2134.5          1920.0          ✅ +10.0%
q65        4321.0          3888.9          ✅ +10.0%
q68        1543.2          1388.8          ✅ +10.0%
q73        2987.6          2688.8          ✅ +10.0%
q79        1765.4          1588.8          ✅ +10.0%
q96        2543.1          2288.7          ✅ +10.0%
------------------------------------------------------------
TOTAL      35318.1         31381.5         ✅ +11.1%

==============================================================
Overall Performance: +11.1% improvement
==============================================================
```

### Parsing Failed (Fallback)
```
==============================================================
QUERY-BY-QUERY COMPARISON
==============================================================

⚠️  Could not extract timing data from logs.
Manual review of logs required.

Baseline queries found: 0
Optimized queries found: 0
```

In this case, users can:
1. Check full benchmark logs manually
2. Adjust parsing patterns in `compare_results.py`
3. Review benchmark output format

## Error Handling

The script gracefully handles:
- ✅ Missing log files
- ✅ Unparseable formats
- ✅ Mismatched queries
- ✅ Missing queries in one run
- ✅ Zero times (skipped queries)

## Integration with Summary

The comparison is embedded in `benchmark-summary.md`:

1. **Performance Improvement** section shows the comparison table
2. **Detailed Results Table** shows markdown table for GitHub
3. **Full Benchmark Logs** section has complete raw logs

## Benefits

1. **Immediate insight**: See which queries improved at a glance
2. **Regression detection**: Spot performance drops immediately
3. **Data export**: CSV format for spreadsheet analysis
4. **Reproducible**: Python script included in artifacts
5. **Flexible**: Works with different benchmark output formats

## Usage in CI/CD

The comparison can be used to:
- **Gate releases**: Fail if overall performance degrades
- **Track progress**: Monitor improvements over time
- **Identify issues**: Find queries that regressed
- **Validate optimizations**: Confirm expected improvements

## Future Enhancements

1. **Historical tracking**: Compare against previous runs
2. **Statistical analysis**: Mean, median, stddev
3. **Visualization**: Generate charts/graphs
4. **Alerts**: Notify on regressions
5. **Query categorization**: Group by query type (CPU vs I/O)
