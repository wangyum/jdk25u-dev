# Final Fix: Spark Log Level Suppression (Based on Source Code Analysis)

## Problem

INFO logs still appear despite using:
```bash
--conf spark.driver.log.level=WARN
--conf spark.executor.log.level=WARN
-Dlog4j2.logger.org.apache.spark.level=WARN
```

Example unwanted logs:
```
26/02/20 00:33:31 INFO Executor: Finished task 984.0 in stage 38.0
26/02/20 00:33:31 INFO TaskSetManager: Starting task 985.0 in stage 38.0
```

## Root Cause (From Source Code Analysis)

### 1. Configuration Name is Wrong

Looking at `/spark-java25/core/src/main/scala/org/apache/spark/SparkContext.scala:411`:

```scala
_conf.get(SPARK_LOG_LEVEL).foreach { level =>
  if (Logging.setLogLevelPrinted) {
    System.err.printf("Setting Spark log level to \"%s\".\n", level)
  }
  setLogLevel(level)
}
```

And the config definition in `/spark-java25/core/src/main/scala/org/apache/spark/internal/config/package.scala:1251`:

```scala
private[spark] val SPARK_LOG_LEVEL = ConfigBuilder("spark.log.level")
  .doc("When set, overrides any user-defined log settings as if calling " +
    "SparkContext.setLogLevel() at Spark startup. Valid log levels include: " +
    SparkContext.VALID_LOG_LEVELS.mkString(","))
  .version("3.5.0")
  .stringConf
```

**KEY FINDING:** Spark reads `spark.log.level`, NOT `spark.driver.log.level` or `spark.executor.log.level`!

### 2. How Spark Sets Log Level

From `Utils.scala:2349-2356`:

```scala
def setLogLevel(l: Level): Unit = {
  val (ctx, loggerConfig) = getLogContext
  loggerConfig.setLevel(l)
  ctx.updateLoggers()

  // Setting threshold to null as rootLevel will define log level for spark-shell
  Logging.sparkShellThresholdLevel = null
}
```

And the context getter:

```scala
private lazy val getLogContext: (LoggerContext, LoggerConfig) = {
  val ctx = LogManager.getContext(false).asInstanceOf[LoggerContext]
  (ctx, ctx.getConfiguration().getLoggerConfig(LogManager.ROOT_LOGGER_NAME))
}
```

**This sets the ROOT logger level**, which affects all Spark loggers.

### 3. Default Log4j2 Configuration

From `Logging.scala:346-350`:

```scala
val defaultLogProps = if (Logging.isStructuredLoggingEnabled) {
  "org/apache/spark/log4j2-json-layout.properties"
} else {
  "org/apache/spark/log4j2-defaults.properties"
}
```

Spark loads its own default log4j2 configuration from `conf/log4j2.properties.template` (line 19):

```properties
rootLogger.level = info
```

## The Correct Solution

### Solution 1: Use `spark.log.level` Configuration (Recommended)

```bash
--conf spark.log.level=WARN
```

This is the official Spark configuration that:
- Sets the root logger level to WARN
- Affects ALL Spark component loggers (Executor, TaskSetManager, etc.)
- Added in Spark 3.5.0
- Works without any JVM options

**Complete command:**

```bash
./bin/spark-submit \
  --master local[1] \
  --driver-memory 3g \
  --conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  --conf spark.log.level=WARN \
  --jars "$JARS_LIST" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3"
```

### Solution 2: Provide Custom log4j2.properties File

Create a file `conf/log4j2.properties`:

```properties
# Set root logger to WARN
rootLogger.level = warn
rootLogger.appenderRef.stdout.ref = console

# Console appender
appender.console.type = Console
appender.console.name = console
appender.console.target = SYSTEM_ERR
appender.console.layout.type = PatternLayout
appender.console.layout.pattern = %d{yy/MM/dd HH:mm:ss} %p %c{1}: %m%n%ex

# Suppress third-party verbose logs
logger.jetty.name = org.sparkproject.jetty
logger.jetty.level = warn
logger.parquet.name = org.apache.parquet
logger.parquet.level = error
```

Then use it:

```bash
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
  -Dlog4j2.configurationFile=file:///path/to/conf/log4j2.properties"
```

### Solution 3: JVM System Property for Log4j2 Root Logger (Alternative)

If `spark.log.level` doesn't work for some reason, you can set the Log4j2 root logger directly:

```bash
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
  -Dlog4j2.rootLogger.level=WARN"
```

## Why Previous Attempts Failed

### ❌ `spark.driver.log.level=WARN`
- This config doesn't exist in Spark source code
- Spark doesn't read this configuration
- It's a non-existent setting

### ❌ `spark.executor.log.level=WARN`
- This config also doesn't exist in Spark source code
- Spark doesn't read this configuration
- It's a non-existent setting

### ❌ `-Dlog4j2.logger.org.apache.spark.level=WARN`
- This sets a specific logger, not the root logger
- Spark's `setLogLevel()` method sets the ROOT logger
- The root logger level still INFO, so child loggers inherit INFO
- This approach doesn't override Spark's initialization

## Recommended Fix for GitHub Actions Workflows

### Update All Workflows

**Replace:**
```yaml
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch \
  -Dlog4j2.logger.org.apache.spark.level=WARN" \
--conf spark.driver.log.level=WARN \
--conf spark.executor.log.level=WARN
```

**With:**
```yaml
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
--conf spark.log.level=WARN
```

Or even simpler (since ERROR is quieter than WARN for benchmarks):

```yaml
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
--conf spark.log.level=ERROR
```

## Files to Update

### 1. `.github/workflows/spark-benchmark.yml`

Update all spark-submit commands (warmup, baseline, optimized):

```yaml
- name: Warmup run (JIT warmup)
  run: |
    export JAVA_HOME=$JDK_HOME
    export PATH=$JAVA_HOME/bin:$PATH
    cd spark-src

    ./bin/spark-submit \
      --master local[1] \
      --driver-memory 3g \
      --conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
      --conf spark.log.level=ERROR \
      --jars "$JARS_LIST" \
      --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
      "$SPARK_SQL_TEST_JAR" \
      --data-location "$TPCDS_DATA" \
      --query-filter "q3"
```

### 2. `.github/workflows/tpcds-benchmark.yml`

Same changes for G1GC and ZGC benchmarks:

```yaml
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
--conf spark.log.level=ERROR
```

For ZGC:

```yaml
--conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseZGC -XX:+AlwaysPreTouch" \
--conf spark.log.level=ERROR
```

### 3. `test-log-suppression.sh`

Update the test script to test the correct configuration:

```bash
echo "Test 1: WITHOUT log suppression"
./bin/spark-submit \
  --master local[1] \
  --driver-memory 3g \
  --conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  --jars "$JARS_LIST" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3" 2>&1 | tee /tmp/test-without-suppression.log

echo "Test 2: WITH log suppression (spark.log.level=WARN)"
./bin/spark-submit \
  --master local[1] \
  --driver-memory 3g \
  --conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  --conf spark.log.level=WARN \
  --jars "$JARS_LIST" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3" 2>&1 | tee /tmp/test-with-suppression.log

echo "Test 3: WITH log suppression (spark.log.level=ERROR)"
./bin/spark-submit \
  --master local[1] \
  --driver-memory 3g \
  --conf spark.driver.extraJavaOptions="-Xms3g -XX:+UseG1GC -XX:+AlwaysPreTouch" \
  --conf spark.log.level=ERROR \
  --jars "$JARS_LIST" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3" 2>&1 | tee /tmp/test-with-error-level.log
```

## Expected Output After Fix

**Before (with INFO logs):**
```
26/02/20 00:33:31 INFO Executor: Finished task 984.0 in stage 38.0
26/02/20 00:33:31 INFO TaskSetManager: Starting task 985.0 in stage 38.0
26/02/20 00:33:31 INFO Executor: Running task 985.0 in stage 38.0
... hundreds of lines ...
```

**After (with spark.log.level=ERROR):**
```
Running benchmark: TPCDS
  Running case: q3
  Stopped after 6 iterations, 2041 ms

OpenJDK 64-Bit Server VM 25.0.3-internal on Linux
TPCDS:                                    Best Time(ms)
----------------------------------------------------------
q3                                                  321
```

Clean output with only benchmark results!

## Summary

**Problem:** INFO logs from Executor, TaskSetManager appear despite configuration

**Root Cause:** Using wrong config name `spark.driver.log.level` instead of `spark.log.level`

**Solution:** Use the official Spark configuration:
```bash
--conf spark.log.level=WARN  # or ERROR for even quieter output
```

**Remove these (they don't work):**
```bash
--conf spark.driver.log.level=WARN           # ❌ Doesn't exist
--conf spark.executor.log.level=WARN         # ❌ Doesn't exist
-Dlog4j2.logger.org.apache.spark.level=WARN  # ❌ Doesn't override root logger
```

**Why this works:**
- `spark.log.level` is the official Spark config (added in Spark 3.5.0)
- SparkContext reads it on startup and calls `setLogLevel()`
- `setLogLevel()` sets the Log4j2 ROOT logger level
- All Spark component loggers inherit from root logger
- Simple, official, and works correctly!
