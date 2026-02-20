#!/bin/bash

# Check if JDK build is complete and valid

set -e

BUILD_DIR=${1:-"build/linux-x86_64-server-release"}

if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: Build directory not found: $BUILD_DIR"
    echo "Usage: $0 [build-directory]"
    echo "Example: $0 build/linux-x86_64-server-release"
    exit 1
fi

JDK_DIR="$BUILD_DIR/jdk"

echo "=================================="
echo "JDK Build Completeness Check"
echo "=================================="
echo "Checking: $JDK_DIR"
echo ""

ERRORS=0

# Function to check file
check_file() {
    local file=$1
    local description=$2
    local critical=${3:-true}

    if [ -f "$file" ] || [ -L "$file" ]; then
        SIZE=$(ls -lh "$file" | awk '{print $5}')
        echo "✅ $description: $SIZE"
        if [ -L "$file" ]; then
            TARGET=$(readlink -f "$file")
            echo "   → Symlink to: $TARGET"
            if [ ! -f "$TARGET" ]; then
                echo "   ⚠️  WARNING: Symlink target doesn't exist!"
                if [ "$critical" = "true" ]; then
                    ERRORS=$((ERRORS + 1))
                fi
            fi
        fi
    else
        if [ "$critical" = "true" ]; then
            echo "❌ $description: MISSING (CRITICAL!)"
            ERRORS=$((ERRORS + 1))
        else
            echo "⚠️  $description: MISSING (optional)"
        fi
    fi
}

echo "Critical Files:"
echo "---------------"
check_file "$JDK_DIR/bin/java" "java executable" true
check_file "$JDK_DIR/lib/modules" "modules jimage" true
check_file "$JDK_DIR/lib/jvm.cfg" "jvm.cfg" true
check_file "$JDK_DIR/lib/server/libjvm.so" "libjvm.so (Linux)" false
check_file "$JDK_DIR/lib/server/libjvm.dylib" "libjvm.dylib (macOS)" false

echo ""
echo "Additional Files:"
echo "-----------------"
check_file "$JDK_DIR/lib/libjli.so" "libjli.so (Linux)" false
check_file "$JDK_DIR/lib/libjli.dylib" "libjli.dylib (macOS)" false
check_file "$JDK_DIR/release" "release file" false

echo ""
echo "=================================="
if [ $ERRORS -eq 0 ]; then
    echo "✅ JDK build is COMPLETE"
    echo "=================================="
    echo ""

    # Try to run java
    echo "Testing java executable:"
    echo "------------------------"
    if "$JDK_DIR/bin/java" -version 2>&1; then
        echo ""
        echo "✅ JDK is functional!"
    else
        echo ""
        echo "❌ JDK executable exists but fails to run!"
        exit 1
    fi

    # Check if modules file is valid
    echo ""
    echo "Validating modules file:"
    echo "------------------------"
    if "$JDK_DIR/bin/java" -XshowSettings:properties -version 2>&1 | grep -q "java.home"; then
        echo "✅ modules file is valid"
    else
        echo "❌ modules file may be corrupted"
        exit 1
    fi

else
    echo "❌ JDK build is INCOMPLETE"
    echo "Errors: $ERRORS critical file(s) missing"
    echo "=================================="
    echo ""
    echo "The build likely failed or was interrupted."
    echo "Try rebuilding:"
    echo "  make clean"
    echo "  bash configure --with-debug-level=release"
    echo "  make images"
    exit 1
fi
