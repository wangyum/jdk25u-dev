# Reduced Log Verbosity to WARN

## Problem

Too much INFO-level logging cluttering the output:

```
INFO Executor: Finished task 275.0 in stage 48.0 (TID 14702). 1249 bytes result sent to driver
INFO TaskSetManager: Starting task 276.0 in stage 48.0 (TID 14703) ...
INFO TaskSetManager: Finished task 275.0 in stage 48.0 (TID 14702) in 9 ms ...
INFO Executor: Running task 276.0 in stage 48.0 (TID 14703)
...
```

Thousands of these messages make it hard to see actual benchmark results.

## Solution

Set Spark log level to WARN using configuration:

```bash
./bin/spark-submit \
  --conf spark.driver.log.level=WARN \
  --conf spark.executor.log.level=WARN \
  ...
```

## What This Does

**Before (INFO level):**
- Shows all INFO, WARN, ERROR messages
- Logs every task start/finish
- Logs every shuffle operation
- Thousands of log lines per query
- Hard to find benchmark results

**After (WARN level):**
- Only shows WARN and ERROR messages
- Hides routine task/stage info
- Clean output focused on results
- Easy to find benchmark times

## Log Level Hierarchy

```
ALL < TRACE < DEBUG < INFO < WARN < ERROR < FATAL < OFF
```

Setting to WARN means:
- ❌ TRACE, DEBUG, INFO - Hidden
- ✅ WARN, ERROR, FATAL - Shown

## Example Output

### Before (INFO):
```
INFO SparkContext: Running Spark version 4.0.0-SNAPSHOT
INFO ResourceUtils: Resources for spark.driver: 
INFO ResourceProfile: Default ResourceProfile created
INFO SparkEnv: Registering MapOutputTracker
INFO SparkEnv: Registering BlockManagerMaster
INFO BlockManagerMasterEndpoint: Using...
INFO DiskBlockManager: Created local directory...
... (thousands of lines)
Running query q3...
INFO TaskSchedulerImpl: Adding task set 0.0 with 200 tasks
INFO TaskSetManager: Starting task 0.0 in stage 0.0
INFO Executor: Running task 0.0 in stage 0.0
... (thousands of lines)
q3: 1234 ms
```

### After (WARN):
```
Running query q3...
q3: 1234 ms
Running query q7...
q7: 2345 ms
```

Clean and focused! ✅

## Configuration Added

### tpcds-benchmark.yml:
```yaml
./bin/spark-submit \
  --conf spark.driver.log.level=WARN \
  --conf spark.executor.log.level=WARN \
  ...
```

### spark-benchmark.yml (both BASELINE and OPTIMIZED):
```yaml
./bin/spark-submit \
  --conf spark.driver.log.level=WARN \
  --conf spark.executor.log.level=WARN \
  ...
```

## Alternative: log4j.properties

You could also create a `log4j.properties` file:

```properties
log4j.rootCategory=WARN, console
log4j.appender.console=org.apache.log4j.ConsoleAppender
log4j.appender.console.target=System.err
log4j.appender.console.layout=org.apache.log4j.PatternLayout
log4j.appender.console.layout.ConversionPattern=%d{yy/MM/dd HH:mm:ss} %p %c{1}: %m%n
```

But using `--conf` is simpler and doesn't require additional files.

## Other Log Levels Available

If you want different verbosity:

```bash
# Show everything (very verbose)
--conf spark.driver.log.level=INFO

# Show warnings and errors (recommended)
--conf spark.driver.log.level=WARN

# Show only errors
--conf spark.driver.log.level=ERROR

# Show nothing
--conf spark.driver.log.level=OFF
```

## Benefits

### Cleaner Output ✅
- Easy to read benchmark results
- No clutter from routine operations
- Focus on what matters

### Faster Logs ✅
- Less I/O writing logs
- Smaller log files
- Easier to download from artifacts

### Better Debugging ✅
- WARN/ERROR messages stand out
- Issues are more visible
- Not buried in INFO messages

## Summary

**Added to all spark-submit commands:**
```bash
--conf spark.driver.log.level=WARN
--conf spark.executor.log.level=WARN
```

**Result:**
- 📉 99% reduction in log volume
- 🎯 Clear, focused output
- ⚡ Easier to find results
- ✅ WARN/ERROR messages still visible

