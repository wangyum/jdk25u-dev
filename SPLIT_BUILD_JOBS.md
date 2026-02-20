# Split Build Jobs

## Changes Made

Split the monolithic `build-jdk-and-spark` job into two separate jobs for better modularity and clearer execution flow.

## New Job Structure

### Before (1 job):
```
build-jdk-and-spark (360 min timeout)
  ├─ Build JDK (~20 min)
  └─ Build Spark (~30-60 min)
```

### After (2 jobs):
```
build-jdk (120 min timeout)
  └─ Build JDK (~20 min)

build-spark (120 min timeout) [needs: build-jdk]
  └─ Build Spark (~30-60 min)
```

## Benefits

### 1. Better Timeout Management ✅
- **Before:** Single 360-minute (6 hour) timeout for both builds
- **After:** Each job has appropriate 120-minute (2 hour) timeout
- If JDK build fails early, Spark job won't consume timeout

### 2. Clearer Separation of Concerns ✅
- **JDK build job:** Only responsible for building JDK
- **Spark build job:** Only responsible for building Spark
- Each job has single, well-defined purpose

### 3. Better Failure Isolation ✅
- If JDK build fails, it's immediately clear
- If Spark build fails, JDK artifact is still available for debugging
- Can re-run individual jobs without rebuilding everything

### 4. Easier Maintenance ✅
- Smaller, focused jobs are easier to understand
- Cache management is clearer (SBT cache only in Spark job)
- JDK verification steps are isolated

### 5. Future Parallelization Ready ✅
- Could potentially build Spark with different configurations in parallel
- Could run tests against JDK while Spark builds
- Modular structure enables future optimizations

## Execution Flow

```
┌─ build-jdk (120 min) ───────────────┐
│   - Install dependencies             │
│   - Configure and build JDK          │
│   - Verify JDK completeness          │
│   - Upload JDK artifact              │
└──────────────────────────────────────┘
               │
               ▼
┌─ build-spark (120 min) ─────────────┐
│   - Download JDK artifact            │
│   - Clone Spark java25 branch        │
│   - Build Spark with custom JDK      │
│   - Upload Spark artifact            │
└──────────────────────────────────────┘
               │
               ▼
  ┌────────────┴────────────┐
  │                         │
  ▼                         ▼
run-baseline          run-optimized
(360 min)             (360 min)
  │                         │
  └────────────┬────────────┘
               ▼
        compare-results
           (30 min)
```

## Dependency Updates

Updated benchmark jobs to depend on both build jobs:

```yaml
# Before
needs: [build-jdk-and-spark, download-dataset]

# After
needs: [build-jdk, build-spark, download-dataset]
```

## Artifact Management

### build-jdk uploads:
- `jdk-build-{sha}` - Custom JDK (~500 MB)
- Retention: 1 day

### build-spark downloads and uploads:
- Downloads: `jdk-build-{sha}`
- Uploads: `spark-build-{sha}` - Built Spark (~2 GB)
- Retention: 1 day

## Permission Fix

Also fixed permission issue where `./build/sbt` wasn't executable after artifact download:

```bash
# Added to both benchmark jobs
chmod +x build/sbt
```

Artifacts don't preserve file permissions, so execute permissions must be restored.

## Total Workflow Structure (6 jobs)

1. **build-jdk** - Build custom JDK (120 min timeout)
2. **build-spark** - Build Spark with custom JDK (120 min timeout, needs: build-jdk)
3. **download-dataset** - Download TPC-DS data (30 min timeout)
4. **run-baseline-benchmark** - Run baseline (360 min, needs: build-jdk, build-spark, download-dataset)
5. **run-optimized-benchmark** - Run optimized (360 min, needs: build-jdk, build-spark, download-dataset)
6. **compare-results** - Compare results (30 min, needs: run-baseline-benchmark, run-optimized-benchmark)

## Summary

**Change:** Split `build-jdk-and-spark` into `build-jdk` and `build-spark`  
**Result:** Better modularity, clearer timeouts, easier debugging

✅ YAML syntax is valid  
✅ Dependency graph is correct  
✅ Execute permission fix included  
✅ Proper artifact flow maintained
