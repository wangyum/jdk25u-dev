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

#include "gc/shared/sparkEscapeAnalysisOptimizer.hpp"
#include "gc/shared/gc_globals.hpp"
#include "logging/log.hpp"
#include "runtime/globals.hpp"
#include "utilities/globalDefinitions.hpp"
#include <cstring>

// Statistics
size_t SparkEscapeAnalysisOptimizer::_total_allocations_eliminated = 0;
size_t SparkEscapeAnalysisOptimizer::_scalar_replacements = 0;
size_t SparkEscapeAnalysisOptimizer::_stack_allocations = 0;
size_t SparkEscapeAnalysisOptimizer::_internal_row_eliminations = 0;
size_t SparkEscapeAnalysisOptimizer::_iterator_eliminations = 0;
size_t SparkEscapeAnalysisOptimizer::_expression_eliminations = 0;
size_t SparkEscapeAnalysisOptimizer::_bytes_saved = 0;

bool SparkEscapeAnalysisOptimizer::is_enabled() {
  return G1OptimizeForSpark && G1SparkEnhanceEscapeAnalysis;
}

void SparkEscapeAnalysisOptimizer::initialize() {
  if (!is_enabled()) {
    return;
  }

  log_info(gc, init)("Spark Escape Analysis Enhancement enabled");
  log_info(gc, init)("  Scalar Replacement Threshold: %zu fields",
                     G1SparkScalarReplacementThreshold);
  log_info(gc, init)("  Stack Allocation Limit: %zu bytes",
                     G1SparkStackAllocationLimit);

  // Reset statistics
  _total_allocations_eliminated = 0;
  _scalar_replacements = 0;
  _stack_allocations = 0;
  _internal_row_eliminations = 0;
  _iterator_eliminations = 0;
  _expression_eliminations = 0;
  _bytes_saved = 0;

  log_debug(gc)("Spark Escape Analysis Optimizer initialized");
}

bool SparkEscapeAnalysisOptimizer::matches_pattern(const char* str, const char* pattern) {
  if (str == nullptr || pattern == nullptr) {
    return false;
  }
  return strstr(str, pattern) != nullptr;
}

bool SparkEscapeAnalysisOptimizer::is_spark_sql_package(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match Spark SQL packages
  return matches_pattern(class_name, "org/apache/spark/sql") ||
         matches_pattern(class_name, "org/apache/spark/catalyst");
}

bool SparkEscapeAnalysisOptimizer::is_internal_row_class(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match InternalRow implementations:
  // - GenericInternalRow
  // - SpecificInternalRow
  // - JoinedRow
  // - MutableProjection
  return is_spark_sql_package(class_name) &&
         (matches_pattern(class_name, "InternalRow") ||
          matches_pattern(class_name, "JoinedRow") ||
          matches_pattern(class_name, "MutableProjection"));
}

bool SparkEscapeAnalysisOptimizer::is_iterator_wrapper_class(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match Spark iterator wrappers:
  // - Scala iterator implementations
  // - Spark SQL iterators
  return is_spark_sql_package(class_name) &&
         (matches_pattern(class_name, "Iterator") ||
          matches_pattern(class_name, "RowIterator"));
}

bool SparkEscapeAnalysisOptimizer::is_expression_eval_class(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match expression evaluation classes:
  // - Expression result holders
  // - Temporary evaluation state
  return is_spark_sql_package(class_name) &&
         (matches_pattern(class_name, "expressions/") ||
          matches_pattern(class_name, "Expression") ||
          matches_pattern(class_name, "Projection"));
}

bool SparkEscapeAnalysisOptimizer::is_aggregation_buffer_class(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match aggregation buffer classes:
  // - AggregationBuffer
  // - AggregateFunction state
  return is_spark_sql_package(class_name) &&
         (matches_pattern(class_name, "AggregationBuffer") ||
          matches_pattern(class_name, "aggregate/") ||
          matches_pattern(class_name, "AggregateFunction"));
}

bool SparkEscapeAnalysisOptimizer::is_columnar_batch_class(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match columnar batch classes:
  // - ColumnarBatch
  // - ColumnVector
  return is_spark_sql_package(class_name) &&
         (matches_pattern(class_name, "ColumnarBatch") ||
          matches_pattern(class_name, "ColumnVector"));
}

bool SparkEscapeAnalysisOptimizer::is_likely_non_escaping_spark_object(const char* class_name,
                                                                         size_t object_size) {
  if (!is_enabled() || class_name == nullptr) {
    return false;
  }

  // Small objects from Spark SQL packages are likely non-escaping
  // Typical pattern: created in hot loop, used locally, discarded
  if (!is_spark_sql_package(class_name)) {
    return false;
  }

  // Size heuristic: small objects (< 1KB) are more likely to be optimized
  if (object_size > 1024) {
    return false;
  }

  // Check for known non-escaping patterns
  if (is_internal_row_class(class_name) ||
      is_iterator_wrapper_class(class_name) ||
      is_expression_eval_class(class_name)) {
    return true;
  }

  return false;
}

bool SparkEscapeAnalysisOptimizer::should_scalarize_aggressively(const char* class_name,
                                                                   size_t field_count) {
  if (!is_enabled()) {
    return false;
  }

  // Scalar replacement: replace object allocation with individual field values
  // Benefit: eliminates allocation entirely, fields become local variables

  // Don't scalarize objects with too many fields
  if (field_count > G1SparkScalarReplacementThreshold) {
    return false;
  }

  // Aggressively scalarize known Spark patterns
  if (is_internal_row_class(class_name)) {
    // InternalRow with few fields: excellent candidate
    return field_count <= 10;
  }

  if (is_expression_eval_class(class_name)) {
    // Expression temporaries: usually have few fields
    return field_count <= 5;
  }

  return false;
}

bool SparkEscapeAnalysisOptimizer::should_attempt_stack_allocation(const char* class_name,
                                                                     size_t object_size) {
  if (!is_enabled()) {
    return false;
  }

  // Stack allocation: allocate on stack instead of heap
  // Benefit: no GC overhead, faster allocation/deallocation

  // Size limit for stack allocation
  if (object_size > G1SparkStackAllocationLimit) {
    return false;
  }

  // Stack allocation for known non-escaping patterns
  if (is_internal_row_class(class_name) && object_size <= 256) {
    // Small InternalRow instances
    return true;
  }

  if (is_iterator_wrapper_class(class_name) && object_size <= 128) {
    // Small iterator wrappers
    return true;
  }

  return false;
}

void SparkEscapeAnalysisOptimizer::record_allocation_eliminated(const char* class_name,
                                                                 size_t size) {
  if (!is_enabled()) {
    return;
  }

  _total_allocations_eliminated++;
  _bytes_saved += size;

  // Classify elimination type
  if (is_internal_row_class(class_name)) {
    _internal_row_eliminations++;
  } else if (is_iterator_wrapper_class(class_name)) {
    _iterator_eliminations++;
  } else if (is_expression_eval_class(class_name)) {
    _expression_eliminations++;
  }

  log_trace(gc)("Spark EA: Eliminated allocation of %s (%zu bytes)",
                class_name, size);
}

void SparkEscapeAnalysisOptimizer::record_scalar_replacement(const char* class_name,
                                                              size_t field_count) {
  _scalar_replacements++;
  log_trace(gc)("Spark EA: Scalar replacement of %s (%zu fields)",
                class_name, field_count);
}

void SparkEscapeAnalysisOptimizer::record_stack_allocation(const char* class_name,
                                                            size_t size) {
  _stack_allocations++;
  log_trace(gc)("Spark EA: Stack allocation of %s (%zu bytes)",
                class_name, size);
}

void SparkEscapeAnalysisOptimizer::print_statistics() {
  if (!is_enabled()) {
    return;
  }

  if (_total_allocations_eliminated == 0) {
    return;
  }

  log_info(gc)("Spark Escape Analysis Statistics:");
  log_info(gc)("  Total allocations eliminated: %zu",
               _total_allocations_eliminated);
  log_info(gc)("  Bytes saved: %zu (%.2f MB)",
               _bytes_saved, _bytes_saved / (1024.0 * 1024.0));
  log_info(gc)("  Scalar replacements: %zu", _scalar_replacements);
  log_info(gc)("  Stack allocations: %zu", _stack_allocations);

  if (_total_allocations_eliminated > 0) {
    double internal_row_percent = 100.0 * _internal_row_eliminations / _total_allocations_eliminated;
    double iterator_percent = 100.0 * _iterator_eliminations / _total_allocations_eliminated;
    double expression_percent = 100.0 * _expression_eliminations / _total_allocations_eliminated;

    log_info(gc)("  Elimination by type:");
    log_info(gc)("    InternalRow: %zu (%.1f%%)",
                 _internal_row_eliminations, internal_row_percent);
    log_info(gc)("    Iterator: %zu (%.1f%%)",
                 _iterator_eliminations, iterator_percent);
    log_info(gc)("    Expression: %zu (%.1f%%)",
                 _expression_eliminations, expression_percent);
  }
}

void SparkEscapeAnalysisOptimizer::log_optimization(const char* phase,
                                                     const char* class_name,
                                                     const char* reason) {
  if (log_is_enabled(Debug, gc)) {
    log_debug(gc)("Spark EA optimization: phase=%s, class=%s, reason=%s",
                  phase, class_name, reason);
  }
}
