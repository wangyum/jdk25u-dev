// Standalone test to verify JVM optimizations work
// No Spark required - pure Java

import java.util.*;

public class JVMOptimizationTest {

    static class TempObject {
        int a, b, c, d, e;
        String s1, s2, s3;

        TempObject(int val, String prefix) {
            this.a = val;
            this.b = val + 1;
            this.c = val + 2;
            this.d = val + 3;
            this.e = val + 4;
            this.s1 = prefix + "_1";
            this.s2 = prefix + "_2";
            this.s3 = prefix + "_3";
        }

        int sum() {
            return a + b + c + d + e + s1.length() + s2.length() + s3.length();
        }
    }

    public static void main(String[] args) {
        System.out.println("=".repeat(80));
        System.out.println("JVM Optimization Test - Standalone Version");
        System.out.println("=".repeat(80));
        System.out.println();

        // Test 1: Heavy allocation (escape analysis benefit)
        test1_HeavyAllocation();

        // Test 2: String deduplication
        test2_StringDedup();

        // Test 3: Hash operations
        test3_HashOperations();

        System.out.println("\nAll tests completed!");
    }

    static void test1_HeavyAllocation() {
        System.out.println("[Test 1] Heavy Allocation (Escape Analysis Test)");
        System.out.println("-".repeat(80));

        long start = System.nanoTime();
        long total = 0;

        // Create many temporary objects that escape analysis can optimize
        for (int i = 0; i < 50_000_000; i++) {
            TempObject obj = new TempObject(i, "temp");
            total += obj.sum();
        }

        long elapsed = (System.nanoTime() - start) / 1_000_000;

        System.out.println("  Allocated 50M temporary objects");
        System.out.println("  Result: " + total);
        System.out.println("  Time: " + elapsed + " ms");
        System.out.println("  Expected improvement with escape analysis: 20-40%");
        System.out.println();
    }

    static void test2_StringDedup() {
        System.out.println("[Test 2] String Deduplication Test");
        System.out.println("-".repeat(80));

        long start = System.nanoTime();

        // Create many duplicate strings
        List<String> strings = new ArrayList<>();
        for (int i = 0; i < 10_000_000; i++) {
            // Only 1000 unique strings, but 10M total
            String s = "category_" + (i % 1000);
            strings.add(s);
        }

        // Force string operations
        long total = 0;
        for (String s : strings) {
            total += s.hashCode();
        }

        long elapsed = (System.nanoTime() - start) / 1_000_000;

        System.out.println("  Created 10M strings (1000 unique)");
        System.out.println("  Hash sum: " + total);
        System.out.println("  Time: " + elapsed + " ms");
        System.out.println("  Expected improvement with string dedup: 10-25%");
        System.out.println();
    }

    static void test3_HashOperations() {
        System.out.println("[Test 3] Hash Operations Test");
        System.out.println("-".repeat(80));

        long start = System.nanoTime();

        // Create hash maps with lots of operations
        Map<Integer, String> map1 = new HashMap<>();
        Map<Integer, String> map2 = new HashMap<>();

        for (int i = 0; i < 5_000_000; i++) {
            map1.put(i, "value_" + i);
        }

        for (int i = 0; i < 5_000_000; i++) {
            map2.put(i, "data_" + i);
        }

        // Join operation (simulate)
        long matches = 0;
        for (Integer key : map1.keySet()) {
            if (map2.containsKey(key)) {
                matches++;
            }
        }

        long elapsed = (System.nanoTime() - start) / 1_000_000;

        System.out.println("  Created 2 hash maps with 5M entries each");
        System.out.println("  Matches: " + matches);
        System.out.println("  Time: " + elapsed + " ms");
        System.out.println("  Expected improvement with hash intrinsics: 5-15%");
        System.out.println("  Note: Full benefit requires MurmurHash3 integration");
        System.out.println();
    }
}
