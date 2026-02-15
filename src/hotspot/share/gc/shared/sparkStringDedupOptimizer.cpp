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

#include "gc/shared/sparkStringDedupOptimizer.hpp"
#include "logging/log.hpp"
#include "runtime/globals.hpp"
#include "utilities/globalDefinitions.hpp"
#include <cstring>
#include <cctype>

// Statistics
size_t SparkStringDedupOptimizer::_total_candidates = 0;
size_t SparkStringDedupOptimizer::_spark_pattern_candidates = 0;
size_t SparkStringDedupOptimizer::_sql_keyword_count = 0;
size_t SparkStringDedupOptimizer::_partition_pattern_count = 0;
size_t SparkStringDedupOptimizer::_column_name_count = 0;

int SparkStringDedupOptimizer::get_age_threshold() {
  if (!is_enabled()) {
    return StringDeduplicationAgeThreshold;
  }

  // Use Spark-specific age threshold (typically 1)
  // This causes strings to be deduplicated immediately after
  // surviving their first minor GC, which is ideal for:
  // - Column names (duplicated millions of times)
  // - SQL query text (reused across tasks)
  // - Partition values (repeated in every row)
  int threshold = G1SparkStringDedupAgeThreshold;

  log_trace(gc, stringdedup)("Spark String Dedup: Using age threshold %d "
                              "(default: %d)",
                              threshold,
                              StringDeduplicationAgeThreshold);

  return threshold;
}

size_t SparkStringDedupOptimizer::get_initial_table_size(size_t default_size) {
  if (!is_enabled()) {
    return default_size;
  }

  // Spark generates massive amounts of strings
  // Use larger initial table to reduce early resizing overhead
  // Multiply by SparkStringDedupTableSizeMultiplier (default 4)
  size_t optimized_size = default_size * G1SparkStringDedupTableSizeMultiplier;

  log_trace(gc, stringdedup)("Spark String Dedup: Initial table size " SIZE_FORMAT
                              " (default: " SIZE_FORMAT ", multiplier: %u)",
                              optimized_size, default_size,
                              G1SparkStringDedupTableSizeMultiplier);

  return optimized_size;
}

double SparkStringDedupOptimizer::get_grow_load_factor(double default_factor) {
  if (!is_enabled()) {
    return default_factor;
  }

  // Slightly higher growth threshold for Spark
  // Allows table to get fuller before growing (reduces memory)
  // Default: 0.9, Spark: 0.95
  double spark_factor = default_factor * 1.05;  // 5% higher

  log_trace(gc, stringdedup)("Spark String Dedup: Grow load factor %.2f "
                              "(default: %.2f)",
                              spark_factor, default_factor);

  return spark_factor;
}

double SparkStringDedupOptimizer::get_shrink_load_factor(double default_factor) {
  if (!is_enabled()) {
    return default_factor;
  }

  // More aggressive shrinking for Spark
  // Spark has bursty string allocation, table should shrink faster
  // Default: 0.3, Spark: 0.25
  double spark_factor = default_factor * 0.85;  // 15% lower

  log_trace(gc, stringdedup)("Spark String Dedup: Shrink load factor %.2f "
                              "(default: %.2f)",
                              spark_factor, default_factor);

  return spark_factor;
}

double SparkStringDedupOptimizer::get_target_load_factor(double default_factor) {
  if (!is_enabled()) {
    return default_factor;
  }

  // Target load factor remains close to default
  // Default: 0.7, Spark: 0.65 (slightly less dense for better lookup performance)
  double spark_factor = default_factor * 0.93;  // 7% lower

  log_trace(gc, stringdedup)("Spark String Dedup: Target load factor %.2f "
                              "(default: %.2f)",
                              spark_factor, default_factor);

  return spark_factor;
}

size_t SparkStringDedupOptimizer::get_cleanup_dead_minimum(size_t default_minimum) {
  if (!is_enabled()) {
    return default_minimum;
  }

  // Lower minimum for cleanup trigger
  // Spark churns through strings quickly, clean more often
  // Default: 1024, Spark: 512
  size_t spark_minimum = default_minimum / 2;

  log_trace(gc, stringdedup)("Spark String Dedup: Cleanup dead minimum " SIZE_FORMAT
                              " (default: " SIZE_FORMAT ")",
                              spark_minimum, default_minimum);

  return spark_minimum;
}

double SparkStringDedupOptimizer::get_cleanup_dead_percent(double default_percent) {
  if (!is_enabled()) {
    return default_percent;
  }

  // Lower percentage threshold for cleanup
  // Trigger cleanup when fewer entries are dead (more aggressive)
  // Default: 5%, Spark: 3%
  double spark_percent = default_percent * 0.6;  // 40% lower

  log_trace(gc, stringdedup)("Spark String Dedup: Cleanup dead percent %.1f%% "
                              "(default: %.1f%%)",
                              spark_percent, default_percent);

  return spark_percent;
}

bool SparkStringDedupOptimizer::is_sql_keyword(const char* str, size_t length) {
  if (str == nullptr || length == 0 || length > 20) {
    return false;  // SQL keywords are typically short
  }

  // Common Spark SQL keywords and operators
  static const char* keywords[] = {
    "SELECT", "FROM", "WHERE", "JOIN", "GROUP", "ORDER", "BY",
    "AND", "OR", "NOT", "IN", "AS", "ON", "INNER", "LEFT", "RIGHT",
    "DISTINCT", "COUNT", "SUM", "AVG", "MAX", "MIN",
    "CAST", "WHEN", "THEN", "ELSE", "END", "CASE"
  };

  // Simple case-insensitive comparison
  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    if (strlen(keywords[i]) == length) {
      bool match = true;
      for (size_t j = 0; j < length; j++) {
        if (toupper(str[j]) != keywords[i][j]) {
          match = false;
          break;
        }
      }
      if (match) {
        return true;
      }
    }
  }

  return false;
}

bool SparkStringDedupOptimizer::is_partition_pattern(const char* str, size_t length) {
  if (str == nullptr || length < 6 || length > 128) {
    return false;  // Partition patterns have typical length range
  }

  // Common Spark partition patterns:
  // - "year=2024"
  // - "month=01"
  // - "day=15"
  // - "country=US"
  // Look for "key=value" pattern
  const char* equals = strchr(str, '=');
  if (equals != nullptr) {
    // Check if left side looks like a partition key
    size_t key_length = equals - str;
    if (key_length > 0 && key_length < 32) {
      // Common partition keys
      if (strncmp(str, "year", 4) == 0 ||
          strncmp(str, "month", 5) == 0 ||
          strncmp(str, "day", 3) == 0 ||
          strncmp(str, "hour", 4) == 0 ||
          strncmp(str, "date", 4) == 0 ||
          strncmp(str, "country", 7) == 0 ||
          strncmp(str, "region", 6) == 0 ||
          strncmp(str, "dt", 2) == 0) {
        return true;
      }
    }
  }

  return false;
}

bool SparkStringDedupOptimizer::is_column_name_pattern(const char* str, size_t length) {
  if (str == nullptr || length == 0 || length > 128) {
    return false;  // Column names are typically short to medium
  }

  // Heuristic: column names are typically:
  // - Short (1-64 characters)
  // - Alphanumeric with underscores
  // - May contain dots (qualified names like "table.column")
  // - No spaces

  bool has_alpha = false;
  bool all_valid_chars = true;

  for (size_t i = 0; i < length; i++) {
    char c = str[i];
    if (isalpha(c)) {
      has_alpha = true;
    } else if (!isdigit(c) && c != '_' && c != '.' && c != '-') {
      all_valid_chars = false;
      break;
    }
  }

  return has_alpha && all_valid_chars && length <= 64;
}

bool SparkStringDedupOptimizer::is_likely_spark_string(const char* str, size_t length) {
  if (str == nullptr || length == 0) {
    return false;
  }

  // Check various Spark string patterns
  if (is_sql_keyword(str, length)) {
    return true;
  }

  if (is_partition_pattern(str, length)) {
    return true;
  }

  if (is_column_name_pattern(str, length)) {
    return true;
  }

  return false;
}

void SparkStringDedupOptimizer::record_dedup_candidate(size_t string_length,
                                                        bool is_spark_pattern) {
  _total_candidates++;

  if (is_spark_pattern) {
    _spark_pattern_candidates++;
  }
}

void SparkStringDedupOptimizer::print_statistics() {
  if (!is_enabled()) {
    return;
  }

  if (_total_candidates == 0) {
    return;
  }

  double spark_pattern_percent = 100.0 * _spark_pattern_candidates / _total_candidates;

  log_info(gc, stringdedup)("Spark String Dedup Statistics:");
  log_info(gc, stringdedup)("  Total candidates: " SIZE_FORMAT, _total_candidates);
  log_info(gc, stringdedup)("  Spark patterns: " SIZE_FORMAT " (%.1f%%)",
                            _spark_pattern_candidates, spark_pattern_percent);
  log_info(gc, stringdedup)("  SQL keywords: " SIZE_FORMAT, _sql_keyword_count);
  log_info(gc, stringdedup)("  Partition patterns: " SIZE_FORMAT, _partition_pattern_count);
  log_info(gc, stringdedup)("  Column names: " SIZE_FORMAT, _column_name_count);
}

void SparkStringDedupOptimizer::log_optimization(const char* phase,
                                                  size_t original_value,
                                                  size_t optimized_value,
                                                  const char* reason) {
  if (log_is_enabled(Debug, gc, stringdedup)) {
    log_debug(gc, stringdedup)("Spark String Dedup optimization: phase=%s, "
                               "original=" SIZE_FORMAT ", optimized=" SIZE_FORMAT ", "
                               "reason=%s",
                               phase, original_value, optimized_value, reason);
  }
}

void SparkStringDedupOptimizer::initialize() {
  if (!is_enabled()) {
    return;
  }

  log_info(gc, init)("Spark String Deduplication Optimization enabled");
  log_info(gc, init)("  Age Threshold: %d", get_age_threshold());
  log_info(gc, init)("  Table Size Multiplier: %u", G1SparkStringDedupTableSizeMultiplier);

  // Reset statistics
  _total_candidates = 0;
  _spark_pattern_candidates = 0;
  _sql_keyword_count = 0;
  _partition_pattern_count = 0;
  _column_name_count = 0;
}
