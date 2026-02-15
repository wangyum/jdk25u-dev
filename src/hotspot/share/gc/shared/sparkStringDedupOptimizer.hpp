/*
 * Copyright (c) 2026, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work, if not, write to the Free Software Foundation,
 * Inc., <ADDRESS>.
 *
 * Please contact Oracle, <ADDRESS>
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_GC_SHARED_SPARKSTRINGDEDUPOPTIMIZER_HPP
#define SHARE_GC_SHARED_SPARKSTRINGDEDUPOPTIMIZER_HPP

#include "memory/allStatic.hpp"
#include "runtime/globals.hpp"
#include "utilities/globalDefinitions.hpp"

class Thread;

// Spark-specific string deduplication optimization
//
// Apache Spark SQL workloads exhibit unique string patterns:
// 1. Column names duplicated across millions of rows
// 2. SQL query text reused across tasks
// 3. Partition values repeated extensively
// 4. Dictionary-encoded strings from Parquet/ORC
//
// This optimizer enhances G1's string deduplication for Spark patterns:
// - Earlier deduplication (lower age threshold)
// - Larger initial hash table for Spark's high string volume
// - More aggressive cleanup for Spark's string churn
// - Pattern detection for common Spark string types

class SparkStringDedupOptimizer : public AllStatic {
public:
  // Check if Spark-optimized string deduplication is enabled
  static bool is_enabled() {
    return UseStringDeduplication && G1OptimizeForSpark && G1SparkAggressiveStringDedup;
  }

  // Get optimized age threshold for string deduplication
  // Spark benefit: Deduplicate strings sooner (age 1 vs default 3)
  // Column names and SQL text should be deduped immediately
  static int get_age_threshold();

  // Get optimized initial table size for string dedup hash table
  // Spark generates massive amounts of strings, needs larger table
  static size_t get_initial_table_size(size_t default_size);

  // Get optimized table load factors for Spark patterns
  // More aggressive growth/shrink for Spark's dynamic string volume
  static double get_grow_load_factor(double default_factor);
  static double get_shrink_load_factor(double default_factor);
  static double get_target_load_factor(double default_factor);

  // Get cleanup thresholds for dead string entries
  // Spark churns through strings quickly, needs aggressive cleanup
  static size_t get_cleanup_dead_minimum(size_t default_minimum);
  static double get_cleanup_dead_percent(double default_percent);

  // Heuristic: Is this likely a Spark-generated string?
  // Detects patterns like:
  // - Short column names (< 64 chars)
  // - SQL keywords and operators
  // - Common partition patterns (year=2024, month=01, etc.)
  static bool is_likely_spark_string(const char* str, size_t length);

  // Logging for string dedup optimizations
  static void log_optimization(const char* phase,
                               size_t original_value,
                               size_t optimized_value,
                               const char* reason);

  // Initialize Spark string dedup optimizer
  // Called during StringDedup::Config::initialize()
  static void initialize();

  // Statistics tracking
  static void record_dedup_candidate(size_t string_length, bool is_spark_pattern);
  static void print_statistics();

private:
  // Pattern detection helpers
  static bool is_sql_keyword(const char* str, size_t length);
  static bool is_partition_pattern(const char* str, size_t length);
  static bool is_column_name_pattern(const char* str, size_t length);

  // Statistics
  static size_t _total_candidates;
  static size_t _spark_pattern_candidates;
  static size_t _sql_keyword_count;
  static size_t _partition_pattern_count;
  static size_t _column_name_count;
};

#endif // SHARE_GC_SHARED_SPARKSTRINGDEDUPOPTIMIZER_HPP
