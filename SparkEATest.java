// Test case to demonstrate aggressive Spark escape analysis
// This simulates common Spark SQL patterns that should now be optimized

class InternalRow {
    int field1;
    int field2;
    int field3;

    InternalRow(int f1, int f2, int f3) {
        this.field1 = f1;
        this.field2 = f2;
        this.field3 = f3;
    }

    int sum() {
        return field1 + field2 + field3;
    }
}

class RowIterator {
    InternalRow row;

    RowIterator(InternalRow r) {
        this.row = r;
    }

    boolean hasNext() {
        return row != null;
    }

    InternalRow next() {
        InternalRow result = row;
        row = null;
        return result;
    }
}

public class SparkEATest {
    // Pattern 1: Object stored in array element (unknown offset)
    // Without Spark EA: NOT scalar replaced
    // With Spark EA: SHOULD be scalar replaced
    static long testArrayPattern(int iterations) {
        long sum = 0;
        InternalRow[] rows = new InternalRow[10];

        for (int i = 0; i < iterations; i++) {
            int idx = i % 10;
            // This creates InternalRow, stores at unknown offset
            // Standard EA sees "unknown offset" and refuses SR
            // Spark EA recognizes InternalRow and allows SR
            rows[idx] = new InternalRow(i, i+1, i+2);
            sum += rows[idx].sum();
        }
        return sum;
    }

    // Pattern 2: Object with fields accessed in loop (unknown field offset)
    // Without Spark EA: NOT scalar replaced
    // With Spark EA: SHOULD be scalar replaced
    static long testFieldPattern(int iterations) {
        long sum = 0;

        for (int i = 0; i < iterations; i++) {
            InternalRow row = new InternalRow(i, i+1, i+2);
            // Access pattern looks like unknown offset in loop
            sum += row.field1 + row.field2 + row.field3;
        }
        return sum;
    }

    // Pattern 3: Multiple control flow paths (multiple bases)
    // Without Spark EA: NOT scalar replaced
    // With Spark EA: SHOULD be scalar replaced
    static long testMultipleBasesPattern(int iterations) {
        long sum = 0;

        for (int i = 0; i < iterations; i++) {
            InternalRow row;
            if (i % 2 == 0) {
                row = new InternalRow(i, 0, 0);
            } else {
                row = new InternalRow(0, i, 0);
            }
            // Standard EA sees multiple possible objects
            // Spark EA recognizes both are InternalRow and allows SR
            sum += row.sum();
        }
        return sum;
    }

    // Pattern 4: Iterator wrapper (appears to escape)
    // Without Spark EA: NOT scalar replaced
    // With Spark EA: SHOULD be scalar replaced
    static long testIteratorPattern(int iterations) {
        long sum = 0;

        for (int i = 0; i < iterations; i++) {
            InternalRow row = new InternalRow(i, i+1, i+2);
            RowIterator iter = new RowIterator(row);

            // Standard EA sees row escaping into iter
            // Spark EA recognizes RowIterator wrapper pattern
            while (iter.hasNext()) {
                sum += iter.next().sum();
            }
        }
        return sum;
    }

    public static void main(String[] args) {
        // Warmup
        System.out.println("Warming up...");
        for (int i = 0; i < 20000; i++) {
            testArrayPattern(100);
            testFieldPattern(100);
            testMultipleBasesPattern(100);
            testIteratorPattern(100);
        }

        // Timed run
        System.out.println("\nRunning timed tests...");
        long start, end;

        start = System.nanoTime();
        long result1 = testArrayPattern(100000);
        end = System.nanoTime();
        System.out.printf("Array pattern: %d (%.2f ms)%n", result1, (end-start)/1_000_000.0);

        start = System.nanoTime();
        long result2 = testFieldPattern(100000);
        end = System.nanoTime();
        System.out.printf("Field pattern: %d (%.2f ms)%n", result2, (end-start)/1_000_000.0);

        start = System.nanoTime();
        long result3 = testMultipleBasesPattern(100000);
        end = System.nanoTime();
        System.out.printf("Multiple bases: %d (%.2f ms)%n", result3, (end-start)/1_000_000.0);

        start = System.nanoTime();
        long result4 = testIteratorPattern(100000);
        end = System.nanoTime();
        System.out.printf("Iterator pattern: %d (%.2f ms)%n", result4, (end-start)/1_000_000.0);

        System.out.println("\nWith aggressive Spark EA, these patterns should show:");
        System.out.println("- Reduced allocation rate");
        System.out.println("- Better CPU cache utilization");
        System.out.println("- Faster execution (stack allocation vs heap)");
        System.out.println("\nTo verify: Run with -XX:+PrintCompilation -XX:+TraceEscapeAnalysis -Xlog:gc=trace");
    }
}
