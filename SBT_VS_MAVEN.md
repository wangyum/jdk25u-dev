# SBT vs Maven for Spark Builds

## Why SBT is Faster for Spark

### Build Speed Comparison

**SBT (Recommended):**
```bash
./build/sbt -Dscala.version=2.13.15 -DskipTests clean package
```
- ✅ Incremental compilation (only recompiles changed files)
- ✅ Parallel compilation across modules
- ✅ Optimized for Scala projects
- ✅ Test jars created automatically
- ⏱️ Build time: ~25-35 minutes

**Maven:**
```bash
./build/mvn -DskipTests clean package test-compile
./build/mvn jar:test-jar -pl sql/catalyst,sql/core
```
- ❌ Full recompilation even for small changes
- ❌ Sequential module compilation by default
- ❌ Less optimized for Scala
- ❌ Requires separate step for test jars
- ⏱️ Build time: ~40-60 minutes

**Speed difference:** SBT is typically **30-50% faster** for Scala projects like Spark.

## Test Jar Generation

### SBT (Automatic)
```bash
./build/sbt -DskipTests clean package
```

**Creates test jars automatically:**
- `sql/core/target/scala-2.13/spark-sql_2.13-4.0.0-SNAPSHOT-tests.jar`
- `sql/catalyst/target/scala-2.13/spark-catalyst_2.13-4.0.0-SNAPSHOT-tests.jar`

**No extra steps needed!**

### Maven (Manual)
```bash
# Step 1: Build and compile tests
./build/mvn -DskipTests clean package test-compile

# Step 2: Create test jars
./build/mvn jar:test-jar -pl sql/catalyst,sql/core
```

**Creates test jars in:**
- `sql/core/target/spark-sql_2.13-4.0.0-SNAPSHOT-tests.jar`
- `sql/catalyst/target/spark-catalyst_2.13-4.0.0-SNAPSHOT-tests.jar`

**Requires two separate commands.**

## Incremental Compilation

### SBT
```bash
# First build
./build/sbt clean package  # 30 minutes

# Modify one file
# Second build
./build/sbt package  # 2-5 minutes (incremental!)
```

SBT tracks file changes and only recompiles what's necessary.

### Maven
```bash
# First build
./build/mvn clean package  # 45 minutes

# Modify one file
# Second build
./build/mvn package  # 40 minutes (recompiles most modules)
```

Maven's incremental compilation is less effective for Scala.

## Cache and State Management

### SBT
- Maintains compilation cache in `.sbt` directory
- Keeps track of dependencies
- Persistent across builds
- Smart about what needs recompilation

### Maven
- Maintains cache in `.m2/repository`
- Downloads dependencies once
- Less intelligent about Scala compilation
- Often recompiles more than necessary

## Parallel Benchmarks on Same Machine

With SBT, we can run both benchmarks in parallel from the same Spark directory:

```bash
# BASELINE
./bin/spark-submit ... &

# OPTIMIZED  
./bin/spark-submit ... &

wait
```

No SBT daemon conflicts because we're using `spark-submit`, not `sbt runMain`.

## Current Workflow Configuration

```yaml
- name: Clone and build Apache Spark (java25 branch)
  run: |
    export JAVA_HOME=$JDK_HOME
    export PATH=$JAVA_HOME/bin:$PATH
    
    git clone --depth 1 --branch java25 https://github.com/wangyum/spark.git spark-src
    cd spark-src
    
    # Build with SBT (fast, automatic test jars)
    ./build/sbt -Dscala.version=2.13.15 -DskipTests clean package
```

```yaml
- name: Run both benchmarks in parallel using spark-submit
  run: |
    cd spark-src
    
    # Find test jars (SBT pattern)
    SPARK_CATALYST_TEST_JAR=$(ls sql/catalyst/target/scala-2.13/spark-catalyst_*-tests.jar | head -1)
    SPARK_SQL_TEST_JAR=$(ls sql/core/target/scala-2.13/spark-sql_*-tests.jar | head -1)
    
    # Use spark-submit (no SBT daemon)
    ./bin/spark-submit --jars "$SPARK_CATALYST_TEST_JAR" ...
```

## Why This Works

1. **Build with SBT** - Fast compilation, automatic test jars
2. **Run with spark-submit** - Direct JVM execution, no SBT daemon
3. **Same directory** - No need to copy Spark
4. **Parallel execution** - Both benchmarks run simultaneously

## Summary

**Use SBT for Spark builds:**
- ✅ 30-50% faster than Maven
- ✅ Automatic test jar creation
- ✅ Better incremental compilation
- ✅ Optimized for Scala
- ✅ Parallel module compilation
- ✅ Spark's official build tool

**Use spark-submit for benchmarks:**
- ✅ No SBT daemon conflicts
- ✅ Direct JVM control
- ✅ Both benchmarks can run in parallel
- ✅ Same Spark directory

**Result:**
- ⏱️ Faster builds (~25-35 min vs ~40-60 min)
- 🚀 True parallel benchmark execution
- 💰 Single runner (cost effective)
- 🎯 Simpler workflow

