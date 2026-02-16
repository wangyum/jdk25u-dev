# Spark MurmurHash3 Intrinsic Implementation Plan

## Challenge

Implementing a full VM intrinsic for MurmurHash3 requires:

1. ✅ C2 Node class (SparkMurmur3HashNode) - **DONE**
2. ✅ Opcode registration - **DONE**
3. ❌ vmIntrinsics.hpp entry - **Requires @IntrinsicCandidate annotation**
4. ❌ Java source modification - **Cannot modify org.apache.spark.sql classes**
5. ❌ Platform-specific assembly - **Complex, architecture-dependent**

## Problem

Spark's UnsafeRow class is external (not part of JDK). We cannot add @IntrinsicCandidate annotations to it.

## Alternative: Inline Hash Optimization

Instead of a full intrinsic, we can optimize at the C2 IR level by:

### Approach 1: Recognize and Optimize Hash Patterns

When C2 compiles UnsafeRow.hashCode():
1. Detect the method signature
2. Replace the call with optimized IR nodes
3. Use existing SIMD/vector instructions where possible

### Approach 2: StubRoutines (Recommended)

Create a JVM runtime stub for MurmurHash3:
- Similar to CRC32, AES intrinsics
- Called from inline_native_hashcode() for Spark classes
- Platform-specific assembly implementations
- No Java modification needed

## Recommended Implementation

### Step 1: Create Runtime Stub

File: `src/hotspot/share/runtime/stubRoutines.hpp`
```cpp
static address _spark_murmur3_hash;
static address spark_murmur3_hash() { return _spark_murmur3_hash; }
```

### Step 2: Generate Stub Code

File: `src/hotspot/cpu/aarch64/stubGenerator_aarch64.cpp` (for ARM)
File: `src/hotspot/cpu/x86/stubGenerator_x86_64.cpp` (for x86)

Implement MurmurHash3_x86_32 in assembly using:
- SIMD registers (XMM/YMM for x86, NEON for ARM)
- Vectorized rotations and multiplications
- Loop unrolling

### Step 3: Call Stub from C2

In `library_call.cpp::inline_native_hashcode()`:

```cpp
if (SparkHashOptimizer::is_enabled() &&
    SparkHashOptimizer::is_unsafe_row_class(class_name)) {

    // Get UnsafeRow base object + offset + size
    Node* base = ...;
    Node* offset = ...;
    Node* length = ...;

    // Call MurmurHash3 stub
    address stub = StubRoutines::spark_murmur3_hash();
    Node* hash_result = make_runtime_call(
        RC_LEAF, OptoRuntime::spark_murmur3_Type(),
        stub, "spark_murmur3_hash", TypePtr::BOTTOM,
        base, offset, length, seed
    );

    set_result(hash_result);
    return true;
}
```

## Performance Expectations

### Without Intrinsic (Current)
- Regular Java hashCode() method
- ~50-100 cycles per row
- No SIMD utilization

### With StubRoutines Intrinsic
- Assembly MurmurHash3 implementation
- ~10-20 cycles per row (5-10x faster)
- SIMD processing of multiple bytes per instruction
- **Expected impact: 20-40% for shuffle-heavy workloads**

## Implementation Complexity

| Approach | Effort | Performance | Portability |
|----------|--------|-------------|-------------|
| Full Intrinsic | Very High | Excellent | Poor (needs Java changes) |
| StubRoutines | Medium | Excellent | Good (per-platform stubs) |
| IR Optimization | Low | Good | Excellent |

## Recommendation

**Implement StubRoutines approach** because:
1. ✅ No Java source modifications needed
2. ✅ Platform-specific optimizations possible
3. ✅ Proven pattern (CRC32, AES, etc.)
4. ✅ Can use SIMD instructions
5. ✅ Works with existing Spark binaries

## Next Steps

1. Create stub entry points in stubRoutines.hpp
2. Implement aarch64 assembly version (for Apple Silicon)
3. Implement x86_64 assembly version
4. Add OptoRuntime type definition
5. Integrate into inline_native_hashcode()
6. Add performance tests

## References

- Existing CRC32 intrinsic: `stubGenerator_*.cpp`
- MurmurHash3 algorithm: https://github.com/aappleby/smhasher
- Spark UnsafeRow: `org.apache.spark.sql.catalyst.expressions.UnsafeRow`
