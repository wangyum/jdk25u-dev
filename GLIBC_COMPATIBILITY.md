# GLIBC Compatibility Guide

## Overview

This JDK build is designed to be compatible with Ubuntu 22.04 and newer versions. GLIBC (GNU C Library) compatibility is crucial for running the JDK across different Linux distributions.

## GLIBC Versions by Ubuntu Release

| Ubuntu Version | GLIBC Version | Compatibility |
|----------------|---------------|---------------|
| Ubuntu 24.04 LTS | 2.39 | ✅ Supported |
| Ubuntu 22.04 LTS | 2.35 | ✅ Supported (build target) |
| Ubuntu 20.04 LTS | 2.31 | ❌ Not compatible |
| Ubuntu 18.04 LTS | 2.27 | ❌ Not compatible |

## Why Build on Ubuntu 22.04?

**Forward Compatibility**: Binaries built on older GLIBC versions run on newer versions, but not vice versa.

- ✅ Build on 22.04 → Runs on 22.04, 24.04, and future versions
- ❌ Build on 24.04 → Only runs on 24.04+

## Check Your GLIBC Version

### Method 1: Quick Check
```bash
ldd --version
```

Output example:
```
ldd (Ubuntu GLIBC 2.35-0ubuntu3.8) 2.35
```

### Method 2: Direct Check
```bash
/lib/x86_64-linux-gnu/libc.so.6
```

### Method 3: Package Info
```bash
dpkg -l | grep libc6
```

### Method 4: Runtime Check
```bash
getconf GNU_LIBC_VERSION
```

## Troubleshooting GLIBC Errors

### Error: "version `GLIBC_2.38' not found"

This means the JDK binary requires GLIBC 2.38+, but your system has an older version.

**Solution 1: Upgrade Ubuntu**
```bash
# Upgrade to Ubuntu 22.04 or 24.04
sudo do-release-upgrade
```

**Solution 2: Build on Your System**
```bash
# Build JDK on your current system
git clone https://github.com/yourusername/jdk25u-dev.git
cd jdk25u-dev
git checkout spark

# Configure (requires JDK 21 as boot JDK)
bash configure --with-boot-jdk=/path/to/jdk21

# Build
make images

# Your JDK will be in build/linux-x86_64-server-release/jdk/
```

**Solution 3: Use Docker**
```bash
# Run in Ubuntu 22.04 container
docker run -it --rm -v $(pwd):/workspace ubuntu:22.04
```

## GitHub Actions Build Configuration

Our CI/CD pipeline builds on **Ubuntu 22.04** to ensure maximum compatibility:

```yaml
jobs:
  build-jdk-and-benchmark:
    runs-on: ubuntu-22.04  # Build on older version for compatibility
```

This ensures the artifact works on:
- ✅ Ubuntu 22.04 LTS (GLIBC 2.35)
- ✅ Ubuntu 24.04 LTS (GLIBC 2.39)
- ✅ Debian 12 (GLIBC 2.36)
- ✅ Future Ubuntu/Debian releases

## Other Linux Distributions

| Distribution | Version | GLIBC | Compatible? |
|--------------|---------|-------|-------------|
| Debian 12 (Bookworm) | Latest | 2.36 | ✅ Yes |
| Debian 11 (Bullseye) | Latest | 2.31 | ❌ No |
| RHEL 9 | Latest | 2.34 | ❌ No (close, may work) |
| RHEL 8 | Latest | 2.28 | ❌ No |
| Fedora 38+ | Latest | 2.37+ | ✅ Yes |
| Alpine Linux | Latest | musl | ❌ No (uses musl, not glibc) |

## Building for Maximum Compatibility

If you need to support older systems (Ubuntu 20.04, RHEL 8, etc.), build on the oldest target:

```bash
# Use Ubuntu 20.04 for widest compatibility
docker run -it ubuntu:20.04
apt-get update && apt-get install -y build-essential git wget
# ... then build JDK
```

This JDK will work on Ubuntu 20.04+ (GLIBC 2.31+).

## Testing Compatibility

```bash
# Check which GLIBC symbols the JDK requires
readelf -V build/linux-x86_64-server-release/jdk/lib/libjli.so | grep GLIBC

# Expected output (for 22.04 build):
# GLIBC_2.2.5, GLIBC_2.3, GLIBC_2.4, ..., GLIBC_2.35
```

## Production Deployment Recommendations

1. **Use LTS versions**: Ubuntu 22.04 LTS or 24.04 LTS
2. **Match build environment**: Build on the oldest version you need to support
3. **Container isolation**: Use Docker/Podman with specific Ubuntu base image
4. **CI/CD artifacts**: Download from GitHub Actions (built on Ubuntu 22.04)

## Summary

- **GitHub Actions builds**: Ubuntu 22.04 (GLIBC 2.35)
- **Minimum supported**: Ubuntu 22.04, Debian 12, Fedora 38+
- **Recommended**: Ubuntu 22.04 LTS or 24.04 LTS
- **Not supported**: Ubuntu 20.04 and older (would need rebuild)

---

**Current Build**: Ubuntu 22.04 (GLIBC 2.35)
**Compatible with**: Ubuntu 22.04, 24.04, and future LTS releases
