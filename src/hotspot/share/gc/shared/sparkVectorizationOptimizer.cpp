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
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., <ADDRESS>.
 *
 * Please contact Oracle, <ADDRESS>
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "gc/shared/sparkVectorizationOptimizer.hpp"
#include "gc/shared/gc_globals.hpp"
#include "logging/log.hpp"
#include "runtime/globals.hpp"
#include "utilities/globalDefinitions.hpp"
#include <cstring>

// Statistics
size_t SparkVectorizationOptimizer::_total_vectorized_operations = 0;
size_t SparkVectorizationOptimizer::_total_elements_processed = 0;
size_t SparkVectorizationOptimizer::_bounds_checks_eliminated = 0;
size_t SparkVectorizationOptimizer::_auto_vectorization_applied = 0;
size_t SparkVectorizationOptimizer::_prefetch_hints_applied = 0;
size_t SparkVectorizationOptimizer::_column_vector_operations = 0;
size_t SparkVectorizationOptimizer::_batch_operations = 0;
size_t SparkVectorizationOptimizer::_dictionary_decodes = 0;
size_t SparkVectorizationOptimizer::_bytes_processed = 0;

bool SparkVectorizationOptimizer::is_enabled() {
  return G1OptimizeForSpark && G1SparkEnableVectorization;
}

void SparkVectorizationOptimizer::initialize() {
  if (!is_enabled()) {
    return;
  }

  log_info(gc, init)("Spark Vectorization Optimization enabled");
  log_info(gc, init)("  Auto-vectorization: %s",
                     G1SparkEnableAutoVectorization ? "enabled" : "disabled");
  log_info(gc, init)("  Bounds check elimination: %s",
                     G1SparkEliminateBoundsChecks ? "enabled" : "disabled");
  log_info(gc, init)("  Prefetch distance: %zu cache lines",
                     G1SparkPrefetchDistance);
  log_info(gc, init)("  Vectorization threshold: %zu elements",
                     G1SparkVectorizationThreshold);

  // Reset statistics
  _total_vectorized_operations = 0;
  _total_elements_processed = 0;
  _bounds_checks_eliminated = 0;
  _auto_vectorization_applied = 0;
  _prefetch_hints_applied = 0;
  _column_vector_operations = 0;
  _batch_operations = 0;
  _dictionary_decodes = 0;
  _bytes_processed = 0;

  log_debug(gc)("Spark Vectorization Optimizer initialized");
}

bool SparkVectorizationOptimizer::matches_pattern(const char* str, const char* pattern) {
  if (str == nullptr || pattern == nullptr) {
    return false;
  }
  return strstr(str, pattern) != nullptr;
}

bool SparkVectorizationOptimizer::is_spark_sql_package(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match Spark SQL and Catalyst packages
  return matches_pattern(class_name, "org/apache/spark/sql") ||
         matches_pattern(class_name, "org/apache/spark/catalyst");
}

bool SparkVectorizationOptimizer::is_column_vector_class(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match ColumnVector implementations:
  // - OnHeapColumnVector
  // - OffHeapColumnVector
  // - ArrowColumnVector
  // - WritableColumnVector
  return is_spark_sql_package(class_name) &&
         (matches_pattern(class_name, "ColumnVector") ||
          matches_pattern(class_name, "WritableColumn"));
}

bool SparkVectorizationOptimizer::is_columnar_batch_class(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match ColumnarBatch and related classes
  return is_spark_sql_package(class_name) &&
         (matches_pattern(class_name, "ColumnarBatch") ||
          matches_pattern(class_name, "ColumnVector"));
}

bool SparkVectorizationOptimizer::is_vectorizable_operation(const char* operation_type,
                                                             size_t elements_count) {
  if (!is_enabled() || operation_type == nullptr) {
    return false;
  }

  // Need minimum elements for vectorization overhead to be worthwhile
  if (elements_count < G1SparkVectorizationThreshold) {
    return false;
  }

  // Vectorizable Spark operations:
  // - Column scans (filter, project)
  // - Batch arithmetic (sum, multiply, etc.)
  // - Comparison operations (equals, greater than)
  // - Dictionary decoding
  // - Null checking on columns

  if (matches_pattern(operation_type, "ColumnVector") ||
      matches_pattern(operation_type, "Batch") ||
      matches_pattern(operation_type, "Filter") ||
      matches_pattern(operation_type, "Project") ||
      matches_pattern(operation_type, "Aggregate") ||
      matches_pattern(operation_type, "Dictionary")) {
    return true;
  }

  return false;
}

bool SparkVectorizationOptimizer::can_eliminate_bounds_checks(const char* loop_context,
                                                               size_t array_length) {
  if (!is_enabled() || !G1SparkEliminateBoundsChecks) {
    return false;
  }

  if (loop_context == nullptr) {
    return false;
  }

  // Safe to eliminate bounds checks in:
  // 1. ColumnarBatch iterations (known size, controlled access)
  // 2. ColumnVector operations (validated at batch creation)
  // 3. Fixed-size loops over columns

  // Must have reasonable array length
  if (array_length == 0 || array_length > 1024 * 1024) {
    return false;
  }

  // Match safe contexts
  if (matches_pattern(loop_context, "ColumnarBatch") ||
      matches_pattern(loop_context, "ColumnVector") ||
      matches_pattern(loop_context, "BatchIterator")) {
    return true;
  }

  return false;
}

size_t SparkVectorizationOptimizer::get_prefetch_distance(size_t column_width) {
  if (!is_enabled()) {
    return 0; // No prefetching
  }

  // G1SparkPrefetchDistance is in cache lines
  // Typical cache line: 64 bytes

  // For narrow columns (1-8 bytes), prefetch further ahead
  // For wide columns (> 64 bytes), prefetch less aggressively

  size_t base_distance = G1SparkPrefetchDistance;

  if (column_width <= 8) {
    // Narrow columns: can prefetch many elements ahead
    return base_distance * 2;
  } else if (column_width <= 32) {
    // Medium columns: standard prefetch
    return base_distance;
  } else {
    // Wide columns: conservative prefetch
    return base_distance / 2;
  }
}

bool SparkVectorizationOptimizer::should_auto_vectorize(const char* loop_type,
                                                         size_t iteration_count) {
  if (!is_enabled() || !G1SparkEnableAutoVectorization) {
    return false;
  }

  if (loop_type == nullptr) {
    return false;
  }

  // Need enough iterations for auto-vectorization
  if (iteration_count < G1SparkVectorizationThreshold) {
    return false;
  }

  // Auto-vectorize these Spark loop patterns:
  // - Column scanning loops
  // - Batch arithmetic operations
  // - Filter application loops
  // - Aggregation loops

  if (matches_pattern(loop_type, "column") ||
      matches_pattern(loop_type, "batch") ||
      matches_pattern(loop_type, "filter") ||
      matches_pattern(loop_type, "scan")) {
    return true;
  }

  return false;
}

void SparkVectorizationOptimizer::record_vectorized_operation(const char* operation_type,
                                                               size_t elements_processed) {
  if (!is_enabled()) {
    return;
  }

  _total_vectorized_operations++;
  _total_elements_processed += elements_processed;

  // Classify operation type for statistics
  if (operation_type != nullptr) {
    if (matches_pattern(operation_type, "ColumnVector")) {
      _column_vector_operations++;
    } else if (matches_pattern(operation_type, "Batch")) {
      _batch_operations++;
    } else if (matches_pattern(operation_type, "Dictionary")) {
      _dictionary_decodes++;
    }
  }

  log_trace(gc)("Spark Vectorization: %s operation, %zu elements",
                operation_type, elements_processed);
}

void SparkVectorizationOptimizer::record_bounds_check_eliminated(size_t count) {
  _bounds_checks_eliminated += count;
  log_trace(gc)("Spark Vectorization: Eliminated %zu bounds checks", count);
}

void SparkVectorizationOptimizer::record_auto_vectorization(const char* loop_name,
                                                            size_t vector_width) {
  _auto_vectorization_applied++;
  log_trace(gc)("Spark Vectorization: Auto-vectorized %s, width=%zu",
                loop_name, vector_width);
}

void SparkVectorizationOptimizer::print_statistics() {
  if (!is_enabled()) {
    return;
  }

  if (_total_vectorized_operations == 0) {
    return;
  }

  log_info(gc)("Spark Vectorization Statistics:");
  log_info(gc)("  Total vectorized operations: %zu",
               _total_vectorized_operations);
  log_info(gc)("  Total elements processed: %zu (%.2f M)",
               _total_elements_processed,
               _total_elements_processed / 1000000.0);
  log_info(gc)("  Bounds checks eliminated: %zu",
               _bounds_checks_eliminated);
  log_info(gc)("  Auto-vectorization applied: %zu",
               _auto_vectorization_applied);
  log_info(gc)("  Prefetch hints applied: %zu",
               _prefetch_hints_applied);

  if (_total_vectorized_operations > 0) {
    double column_percent = 100.0 * _column_vector_operations / _total_vectorized_operations;
    double batch_percent = 100.0 * _batch_operations / _total_vectorized_operations;
    double dict_percent = 100.0 * _dictionary_decodes / _total_vectorized_operations;

    log_info(gc)("  Operation breakdown:");
    log_info(gc)("    ColumnVector: %zu (%.1f%%)",
                 _column_vector_operations, column_percent);
    log_info(gc)("    Batch operations: %zu (%.1f%%)",
                 _batch_operations, batch_percent);
    log_info(gc)("    Dictionary decodes: %zu (%.1f%%)",
                 _dictionary_decodes, dict_percent);
  }
}

void SparkVectorizationOptimizer::log_optimization(const char* phase,
                                                   const char* class_name,
                                                   const char* reason) {
  if (log_is_enabled(Debug, gc)) {
    log_debug(gc)("Spark Vectorization: phase=%s, class=%s, reason=%s",
                  phase, class_name, reason);
  }
}
