# Fix: Spark Still Shows INFO Logs Despite WARN Setting

## Problem

Even with these settings:
```bash
--conf spark.driver.log.level=WARN
--conf spark.executor.log.level=WARN
```

You still see INFO logs:
```
26/02/20 00:33:31 INFO Executor: Finished task 984.0 in stage 38.0
26/02/20 00:33:31 INFO TaskSetManager: Starting task 985.0 in stage 38.0
26/02/20 00:33:31 INFO Executor: Running task 985.0 in stage 38.0
```

## Root Cause

The `spark.driver.log.level` and `spark.executor.log.level` settings only control **some** loggers, not all Spark components. Specifically:
- ❌ Doesn't affect: Executor, TaskSetManager, DAGScheduler, etc.
- ✅ Only affects: High-level Spark Context logs

## Solution 1: Use Log4j Configuration (Recommended)

### Option A: Via Java System Properties

```bash
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
  -Dlog4j2.configurationFile=file:///path/to/log4j2.properties"
```

Create `log4j2.properties`:
```properties
# Set root logger to WARN
rootLogger.level = WARN
rootLogger.appenderRef.console.ref = console

# Console appender
appender.console.type = Console
appender.console.name = console
appender.console.target = SYSTEM_ERR
appender.console.layout.type = PatternLayout
appender.console.layout.pattern = %d{yy/MM/dd HH:mm:ss} %p %c{1}: %m%n

# Suppress specific noisy loggers
logger.executor.name = org.apache.spark.executor.Executor
logger.executor.level = WARN

logger.tasksetmanager.name = org.apache.spark.scheduler.TaskSetManager
logger.tasksetmanager.level = WARN

logger.taskscheduler.name = org.apache.spark.scheduler.TaskSchedulerImpl
logger.taskscheduler.level = WARN
```

### Option B: Inline Log4j Configuration (Simpler)

```bash
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
  -Dlog4j2.rootLogger.level=WARN \
  -Dlog4j2.logger.executor.name=org.apache.spark.executor.Executor \
  -Dlog4j2.logger.executor.level=WARN \
  -Dlog4j2.logger.taskset.name=org.apache.spark.scheduler.TaskSetManager \
  -Dlog4j2.logger.taskset.level=WARN"
```

### Option C: Set All Spark Loggers to WARN (Easiest)

```bash
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
  -Dlog4j2.logger.org.apache.spark.level=WARN"
```

This sets ALL `org.apache.spark.*` loggers to WARN level.

## Solution 2: Using spark-defaults.conf (For Permanent Settings)

Edit `$SPARK_HOME/conf/spark-defaults.conf`:

```properties
spark.driver.extraJavaOptions=-Dlog4j2.logger.org.apache.spark.level=WARN
```

## Solution 3: Programmatic Configuration (In Spark Code)

If you have access to modify the Spark job code:

```scala
import org.apache.log4j.{Level, Logger}

// Set all Spark loggers to WARN
Logger.getLogger("org.apache.spark").setLevel(Level.WARN)
Logger.getLogger("org.apache.spark.executor.Executor").setLevel(Level.WARN)
Logger.getLogger("org.apache.spark.scheduler.TaskSetManager").setLevel(Level.WARN)
```

## Recommended Fix for GitHub Actions Workflows

### Current (Doesn't Fully Work)
```yaml
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
--conf spark.driver.log.level=WARN \
--conf spark.executor.log.level=WARN
```

### Fixed (Suppresses All INFO Logs)
```yaml
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
  -Dlog4j2.logger.org.apache.spark.level=WARN"
```

**Note:** We can remove the separate `--conf spark.driver.log.level=WARN` since Log4j2 config takes precedence.

## Complete Updated Command

```bash
./bin/spark-submit \
  --master local[1] \
  --driver-memory 3g \
  --conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
    -Dlog4j2.logger.org.apache.spark.level=WARN" \
  --jars "$JARS_LIST" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3" \
  | tee benchmark.log
```

## Specific Loggers to Suppress

If you want fine-grained control, suppress these specific noisy loggers:

```bash
-Dlog4j2.logger.executor.name=org.apache.spark.executor.Executor \
-Dlog4j2.logger.executor.level=WARN \
-Dlog4j2.logger.taskset.name=org.apache.spark.scheduler.TaskSetManager \
-Dlog4j2.logger.taskset.level=WARN \
-Dlog4j2.logger.taskscheduler.name=org.apache.spark.scheduler.TaskSchedulerImpl \
-Dlog4j2.logger.taskscheduler.level=WARN \
-Dlog4j2.logger.dagscheduler.name=org.apache.spark.scheduler.DAGScheduler \
-Dlog4j2.logger.dagscheduler.level=WARN \
-Dlog4j2.logger.blockmanager.name=org.apache.spark.storage.BlockManager \
-Dlog4j2.logger.blockmanager.level=WARN
```

But the simplest is just:
```bash
-Dlog4j2.logger.org.apache.spark.level=WARN
```

## Testing the Fix

### Test Locally
```bash
cd /Users/yumwang/opensource/spark-java25

./bin/spark-submit \
  --master local[1] \
  --driver-memory 3g \
  --conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
    -Dlog4j2.logger.org.apache.spark.level=WARN" \
  --jars "$(ls core/target/scala-2.13/spark-core_2.13-*-tests.jar | head -1),$(ls sql/catalyst/target/scala-2.13/spark-catalyst_2.13-*-tests.jar | head -1)" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$(ls sql/core/target/scala-2.13/spark-sql_2.13-*-tests.jar | head -1)" \
  --data-location "/Users/yumwang/opensource/spark-sql-perf/tpcds_5GB" \
  --query-filter "q3" 2>&1 | grep INFO
```

**Expected:** No INFO logs, or very few system-level INFO logs only.

## Why This Works

**Log4j2 Logger Hierarchy:**
```
org.apache.spark                    ← Set this to WARN
  ├─ org.apache.spark.executor
  │    └─ Executor                   ← Inherits WARN
  ├─ org.apache.spark.scheduler
  │    ├─ TaskSetManager             ← Inherits WARN
  │    ├─ TaskSchedulerImpl          ← Inherits WARN
  │    └─ DAGScheduler               ← Inherits WARN
  └─ org.apache.spark.storage
       └─ BlockManager               ← Inherits WARN
```

By setting the parent logger `org.apache.spark` to WARN, all child loggers inherit this level.

## Comparison of Approaches

| Approach | Effectiveness | Simplicity | Recommended |
|----------|---------------|------------|-------------|
| `spark.driver.log.level=WARN` | ❌ Partial | ✅ Easy | ❌ No |
| Log4j2 file config | ✅ Complete | ❌ Complex | ⚠️ For production |
| Inline Log4j2 `-D` flags | ✅ Complete | ✅ Easy | ✅ Yes |
| Programmatic (Scala) | ✅ Complete | ❌ Requires code change | ❌ No |

## Expected Output After Fix

**Before (with INFO logs):**
```
26/02/20 00:33:31 INFO Executor: Finished task 984.0
26/02/20 00:33:31 INFO TaskSetManager: Starting task 985.0
26/02/20 00:33:31 INFO Executor: Running task 985.0
...hundreds of lines...
```

**After (only benchmark results):**
```
Running benchmark: TPCDS
  Running case: q3
  Stopped after 6 iterations, 2041 ms

OpenJDK 64-Bit Server VM 25.0.3-internal on Linux
TPCDS:                                    Best Time(ms)
----------------------------------------------------------
q3                                                  321
```

Much cleaner output!

## Implementation for GitHub Actions

Update all workflow steps to include the Log4j2 flag:

```yaml
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
  -Dlog4j2.logger.org.apache.spark.level=WARN"
```

You can also keep `--conf spark.driver.log.level=WARN` for compatibility, but it's not necessary with the Log4j2 setting.

## Summary

**Problem:** Spark INFO logs still appear despite `spark.driver.log.level=WARN`

**Root Cause:** That setting doesn't control all Spark component loggers

**Solution:** Add to extraJavaOptions:
```bash
-Dlog4j2.logger.org.apache.spark.level=WARN
```

**Full command:**
```bash
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
  -Dlog4j2.logger.org.apache.spark.level=WARN"
```

This will suppress all INFO logs from Spark components and only show WARN/ERROR messages!
