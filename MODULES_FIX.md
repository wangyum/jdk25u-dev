# Fix: Missing lib/modules File

## Problem

SBT and the JDK fail with:
```
java.nio.file.NoSuchFileException: .../jdk/lib/modules
```

This file is **critical** - it contains all Java platform modules and is required for the JDK to run.

## Root Cause

The workflows were using the wrong build directory:

**Wrong:**
```bash
build/linux-x86_64-server-release/jdk/           # Incomplete - missing modules file
```

**Correct:**
```bash
build/linux-x86_64-server-release/images/jdk/    # Complete - has modules file
```

## Why This Happens

When you run `make images`, the JDK build system creates:

1. **Intermediate build artifacts** in `build/*/jdk/` (incomplete)
2. **Final runtime image** in `build/*/images/jdk/` (complete with modules file)

The `images/jdk/` directory contains the fully assembled JDK with:
- ✅ `lib/modules` - Module image file (CRITICAL)
- ✅ `lib/jvm.cfg` - JVM configuration
- ✅ `lib/server/libjvm.so` - JVM library
- ✅ All other runtime files

## Files Changed

### 1. `.github/workflows/spark-benchmark.yml`
```yaml
# Before:
echo "JDK_HOME=$(pwd)/build/linux-x86_64-server-release/jdk" >> $GITHUB_ENV

# After:
echo "JDK_HOME=$(pwd)/build/linux-x86_64-server-release/images/jdk" >> $GITHUB_ENV
```

Also updated:
- Packaging directory: `cd build/linux-x86_64-server-release/images`
- Upload paths: `build/.../images/jdk-spark-optimized-benchmark.tar.gz`

### 2. `.github/workflows/quick-jdk-build.yml`
```yaml
# Before:
echo "BUILD_DIR=$(pwd)/build/linux-x86_64-server-release" >> $GITHUB_ENV

# After:
echo "BUILD_DIR=$(pwd)/build/linux-x86_64-server-release/images" >> $GITHUB_ENV
```

### 3. Added Verification Steps

Both workflows now verify critical files exist:
```bash
- name: Verify JDK build completeness
  run: |
    # Check modules file exists
    if [ ! -f "$JDK_HOME/lib/modules" ]; then
      echo "ERROR: lib/modules file not found!"
      exit 1
    fi
    # Check other critical files
    ...
```

## Testing Locally

Use the provided script to check your build:

```bash
./check-jdk-build.sh build/linux-x86_64-server-release/images
```

Or check manually:
```bash
# Check if modules file exists
ls -lh build/linux-x86_64-server-release/images/jdk/lib/modules

# Test the JDK
build/linux-x86_64-server-release/images/jdk/bin/java -version
```

## How to Build Correctly

Always use `make images`:

```bash
# Clean build
make clean

# Configure
bash configure \
  --with-debug-level=release \
  --with-native-debug-symbols=none

# Build (creates images/jdk/)
make images

# Verify
ls -lh build/linux-x86_64-server-release/images/jdk/lib/modules
```

## Common Mistakes

❌ **Don't use:**
```bash
make jdk          # Incomplete - no modules file
make exploded-image  # Development only
```

✅ **Always use:**
```bash
make images       # Complete JDK with modules file
```

## Impact

This fix resolves:
- ✅ SBT compilation errors
- ✅ JDK startup failures
- ✅ Missing modules file in packaged tar.gz
- ✅ "could not open lib/modules" errors

## Verification

After this fix, the workflows will:
1. Build JDK with `make images`
2. Verify `lib/modules` file exists
3. Package complete JDK from `images/jdk/` directory
4. Upload tar.gz with all necessary files

The packaged JDK will now work correctly when extracted and run.
