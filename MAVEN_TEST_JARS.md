# Generating Test Jars with Maven

## Problem

Maven's `package` goal with `-DskipTests` doesn't create test jars by default. The `-DskipTests` flag:
- Skips test **execution** ✅
- But also skips test **compilation** ❌

Without compiled tests, there are no test jars to package.

## Solution

Use a two-step process:

### Step 1: Build Spark and Compile Tests
```bash
./build/mvn -DskipTests clean package test-compile
```

**What this does:**
- `clean` - Clean previous builds
- `package` - Build main jars
- `test-compile` - Compile test sources (but don't run them)
- `-DskipTests` - Skip test execution

### Step 2: Create Test Jars
```bash
./build/mvn jar:test-jar -pl sql/catalyst,sql/core
```

**What this does:**
- `jar:test-jar` - Create jars from compiled test classes
- `-pl sql/catalyst,sql/core` - Only for these modules (faster)

## Maven vs SBT Jar Naming

### SBT (old):
```
sql/core/target/scala-2.13/spark-sql_2.13-4.0.0-SNAPSHOT-tests.jar
sql/catalyst/target/scala-2.13/spark-catalyst_2.13-4.0.0-SNAPSHOT-tests.jar
```

### Maven (new):
```
sql/core/target/spark-sql_2.13-4.0.0-SNAPSHOT-tests.jar
sql/catalyst/target/spark-catalyst_2.13-4.0.0-SNAPSHOT-tests.jar
```

Or sometimes:
```
sql/core/target/scala-2.13/spark-sql_2.13-4.0.0-SNAPSHOT-tests.jar
```

## Finding Test Jars (Flexible Pattern Matching)

The workflow now uses flexible pattern matching:

```bash
# Try Maven pattern first (target directory)
SPARK_SQL_TEST_JAR=$(ls sql/core/target/spark-sql_*-tests.jar 2>/dev/null | head -1)

# Fallback to Scala subdirectory pattern
if [ -z "$SPARK_SQL_TEST_JAR" ]; then
  SPARK_SQL_TEST_JAR=$(ls sql/core/target/scala-*/spark-sql_*-tests.jar 2>/dev/null | head -1)
fi

# Verify jar exists
if [ -z "$SPARK_SQL_TEST_JAR" ] || [ ! -f "$SPARK_SQL_TEST_JAR" ]; then
  echo "ERROR: test jar not found!"
  find sql/core/target -name "*-tests.jar"
  exit 1
fi
```

## Alternative Approaches

### Option 1: Compile tests without skipping (slower)
```bash
./build/mvn clean package
# Tests are compiled and executed
# Test jars should be created if configured in pom.xml
```

**Pros:** Simple one command
**Cons:** Runs all tests (very slow, ~hours)

### Option 2: Use install instead of package
```bash
./build/mvn -DskipTests clean install
```

**Pros:** Installs to local Maven repo
**Cons:** May or may not create test jars depending on pom.xml configuration

### Option 3: Current approach (Recommended)
```bash
./build/mvn -DskipTests clean package test-compile
./build/mvn jar:test-jar -pl sql/catalyst,sql/core
```

**Pros:** 
- ✅ Fast (skips test execution)
- ✅ Explicit (clear what's being built)
- ✅ Targeted (only needed modules)

**Cons:**
- Two commands instead of one

## Verification

After building, verify test jars exist:

```bash
ls -lh sql/core/target/*-tests.jar
ls -lh sql/catalyst/target/*-tests.jar
```

Expected output:
```
-rw-r--r-- 1 runner runner 15M spark-sql_2.13-4.0.0-SNAPSHOT-tests.jar
-rw-r--r-- 1 runner runner 12M spark-catalyst_2.13-4.0.0-SNAPSHOT-tests.jar
```

## Using Test Jars with spark-submit

Once test jars are created:

```bash
./bin/spark-submit \
  --jars "$SPARK_CATALYST_TEST_JAR" \
  --class org.apache.spark.sql.execution.benchmark.TPCDSQueryBenchmark \
  "$SPARK_SQL_TEST_JAR" \
  --data-location "$TPCDS_DATA" \
  --query-filter "q3,q7,..."
```

The benchmark class is in the test jar, so we use it as the main jar.

## Summary

**Build command:**
```bash
./build/mvn -DskipTests clean package test-compile
./build/mvn jar:test-jar -pl sql/catalyst,sql/core
```

**Find jars:**
```bash
SPARK_CATALYST_TEST_JAR=$(ls sql/catalyst/target/spark-catalyst_*-tests.jar | head -1)
SPARK_SQL_TEST_JAR=$(ls sql/core/target/spark-sql_*-tests.jar | head -1)
```

**Result:**
- ✅ Test jars created without running tests
- ✅ Fast build (~35 min vs hours with tests)
- ✅ Works with Maven build system
- ✅ Compatible with spark-submit

