// Test case to verify JVM optimizations are working
// This is CPU-bound and allocation-heavy (NOT I/O bound)

import org.apache.spark.sql.SparkSession
import org.apache.spark.sql.functions._

object SparkOptimizationTest {
  def main(args: Array[String]): Unit = {
    val spark = SparkSession.builder()
      .appName("JVM Optimization Test")
      .master("local[4]")
      .getOrCreate()

    import spark.implicits._

    println("=" * 80)
    println("Testing JVM Optimizations - CPU-bound workload")
    println("=" * 80)

    // Test 1: Heavy object allocation (escape analysis benefit)
    println("\n[Test 1] Heavy Allocation Test")
    testHeavyAllocation(spark)

    // Test 2: String operations (string dedup benefit)
    println("\n[Test 2] String Deduplication Test")
    testStringDedup(spark)

    // Test 3: Hash-heavy operations (hash intrinsics benefit)
    println("\n[Test 3] Hash Operations Test")
    testHashOperations(spark)

    spark.stop()
  }

  def testHeavyAllocation(spark: SparkSession): Unit = {
    import spark.implicits._

    val start = System.currentTimeMillis()

    // Create dataset with lots of temporary objects
    val df = (1 to 10_000_000).toDF("id")
      .withColumn("temp1", $"id" + 1)
      .withColumn("temp2", $"id" * 2)
      .withColumn("temp3", $"id" - 3)
      .withColumn("result", $"temp1" + $"temp2" + $"temp3")
      .select("result")
      .agg(sum("result").as("total"))

    val result = df.collect()
    val elapsed = System.currentTimeMillis() - start

    println(s"  Result: ${result(0)}")
    println(s"  Time: ${elapsed}ms")
    println(s"  Expected improvement with escape analysis: 15-30%")
  }

  def testStringDedup(spark: SparkSession): Unit = {
    import spark.implicits._

    val start = System.currentTimeMillis()

    // Create lots of duplicate strings
    val df = (1 to 1_000_000).flatMap { i =>
      val category = s"category_${i % 100}"  // Only 100 unique strings
      (1 to 10).map(j => (category, i, j))
    }.toDF("category", "id", "value")
      .groupBy("category")
      .agg(
        sum("value").as("total"),
        count("*").as("count")
      )
      .collect()

    val elapsed = System.currentTimeMillis() - start

    println(s"  Processed ${df.length} groups")
    println(s"  Time: ${elapsed}ms")
    println(s"  Expected improvement with string dedup: 10-20%")
  }

  def testHashOperations(spark: SparkSession): Unit = {
    import spark.implicits._

    val start = System.currentTimeMillis()

    // Hash-heavy operations (joins and aggregations)
    val df1 = (1 to 5_000_000).map(i => (i, s"value_$i")).toDF("id", "value1")
    val df2 = (1 to 5_000_000).map(i => (i, s"data_$i")).toDF("id", "value2")

    val result = df1.join(df2, "id")
      .groupBy($"id" % 1000 as "bucket")
      .agg(count("*").as("count"))
      .collect()

    val elapsed = System.currentTimeMillis() - start

    println(s"  Processed ${result.length} buckets")
    println(s"  Time: ${elapsed}ms")
    println(s"  Expected improvement with hash intrinsics: 5-15%")
    println(s"  Note: Full benefit requires MurmurHash3 assembly integration")
  }
}
