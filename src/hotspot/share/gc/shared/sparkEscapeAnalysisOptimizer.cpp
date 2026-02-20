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

// Deoptimization tracking - adaptive behavior
size_t SparkEscapeAnalysisOptimizer::_deoptimization_count = 0;
size_t SparkEscapeAnalysisOptimizer::_optimization_attempts = 0;
bool SparkEscapeAnalysisOptimizer::_adaptive_mode_disabled = false;

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

bool SparkEscapeAnalysisOptimizer::matches_exact_class(const char* class_name, const char* target_class) {
  if (class_name == nullptr || target_class == nullptr) {
    return false;
  }
  // Match exact class name or class name with package prefix
  const char* match = strstr(class_name, target_class);
  if (match == nullptr) {
    return false;
  }
  // Ensure it's not a partial match (e.g., "InternalRow" shouldn't match "MyInternalRowImpl")
  // Check that the match is either at the start or preceded by '/' (package separator)
  if (match != class_name && *(match - 1) != '/') {
    return false;
  }
  // Check that the match ends the string or is followed by '$' (inner class) or ';' (end)
  size_t target_len = strlen(target_class);
  char next_char = match[target_len];
  return next_char == '\0' || next_char == '$' || next_char == ';';
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

  // Match SPECIFIC InternalRow implementations with exact matching:
  // - GenericInternalRow
  // - SpecificInternalRow
  // - JoinedRow (often escapes, be conservative)
  // - UnsafeRow (often escapes to storage)
  //
  // Use exact matching to avoid false positives
  return is_spark_sql_package(class_name) &&
         (matches_exact_class(class_name, "GenericInternalRow") ||
          matches_exact_class(class_name, "SpecificInternalRow"));
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

  // CRITICAL FIX: Check if adaptive mode has disabled optimizations due to high deopt rate
  if (_adaptive_mode_disabled) {
    log_trace(gc)("Spark EA: Disabled due to high deoptimization rate");
    return false;
  }

  // CRITICAL FIX: Conservative approach - only optimize if deopt rate is acceptable
  // If we've attempted many optimizations and have high deopt rate, back off
  if (_optimization_attempts > 100) {
    double deopt_rate = (double)_deoptimization_count / _optimization_attempts;
    if (deopt_rate > 0.10) {  // More than 10% deopt rate is too high
      _adaptive_mode_disabled = true;
      log_warning(gc)("Spark EA: Disabling optimizations due to high deopt rate: %.1f%%",
                      deopt_rate * 100);
      return false;
    }
  }

  // CRITICAL FIX: Be MUCH more conservative with size heuristic
  // TPC-DS q3 workload showed that even small objects can escape and cause deopt
  // Only optimize VERY small objects that are truly temporary
  if (object_size > 256) {  // Reduced from 1KB to 256 bytes
    return false;
  }

  if (!is_spark_sql_package(class_name)) {
    return false;
  }

  // CRITICAL FIX: Only optimize specific known-safe patterns
  // Don't optimize based on vague patterns like "has Iterator in name"
  // Only optimize exact matches that we're confident about
  if (is_internal_row_class(class_name)) {
    // Only very small InternalRow instances that are likely temporaries
    if (object_size <= 128) {
      _optimization_attempts++;
      return true;
    }
  }

  // CRITICAL FIX: Removed iterator and expression optimizations
  // These caused regressions in TPC-DS q3, too many false positives

  return false;
}

bool SparkEscapeAnalysisOptimizer::should_scalarize_aggressively(const char* class_name,
                                                                   size_t field_count) {
  if (!is_enabled()) {
    return false;
  }

  // CRITICAL FIX: Check adaptive mode before attempting scalar replacement
  if (_adaptive_mode_disabled) {
    return false;
  }

  // Scalar replacement: replace object allocation with individual field values
  // Benefit: eliminates allocation entirely, fields become local variables

  // CRITICAL FIX: Much more conservative threshold
  // Don't scalarize objects with too many fields - overhead increases with field count
  if (field_count > G1SparkScalarReplacementThreshold) {
    return false;
  }

  // CRITICAL FIX: Only scalarize VERY small objects
  // TPC-DS q3 regression showed that aggressive scalarization causes deopt overhead
  if (is_internal_row_class(class_name)) {
    // CRITICAL FIX: Reduced from <= 10 to <= 4 fields
    // Only tiny InternalRow instances benefit, larger ones cause deopt
    if (field_count <= 4) {
      _optimization_attempts++;
      return true;
    }
  }

  // CRITICAL FIX: Removed expression eval scalarization
  // Caused too many deoptimizations in real workloads

  return false;
}

bool SparkEscapeAnalysisOptimizer::should_attempt_stack_allocation(const char* class_name,
                                                                     size_t object_size) {
  if (!is_enabled()) {
    return false;
  }

  // CRITICAL FIX: Check adaptive mode
  if (_adaptive_mode_disabled) {
    return false;
  }

  // Stack allocation: allocate on stack instead of heap
  // Benefit: no GC overhead, faster allocation/deallocation

  // CRITICAL FIX: Conservative size limit
  // Stack allocation has overhead and can cause deopt if assumptions fail
  if (object_size > G1SparkStackAllocationLimit) {
    return false;
  }

  // CRITICAL FIX: Only attempt stack allocation for VERY small, confirmed-safe objects
  // TPC-DS q3 showed that aggressive stack allocation causes regression
  if (is_internal_row_class(class_name) && object_size <= 64) {
    // CRITICAL FIX: Reduced from 256 to 64 bytes - only tiny temporary rows
    _optimization_attempts++;
    return true;
  }

  // CRITICAL FIX: Removed iterator wrapper stack allocation
  // Iterators often escape or have complex lifecycle, caused deopt

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

void SparkEscapeAnalysisOptimizer::record_deoptimization(const char* class_name,
                                                          const char* reason) {
  _deoptimization_count++;

  log_debug(gc)("Spark EA: Deoptimization of %s, reason: %s (total deopts: %zu, attempts: %zu)",
                class_name, reason, _deoptimization_count, _optimization_attempts);

  // Check if we should disable adaptive mode
  if (_optimization_attempts > 50) {
    double deopt_rate = (double)_deoptimization_count / _optimization_attempts;
    if (deopt_rate > 0.15) {
      _adaptive_mode_disabled = true;
      log_warning(gc)("Spark EA: DISABLING optimizations due to high deopt rate: %.1f%% (%zu deopts / %zu attempts)",
                      deopt_rate * 100, _deoptimization_count, _optimization_attempts);
    }
  }
}

void SparkEscapeAnalysisOptimizer::print_statistics() {
  if (!is_enabled()) {
    return;
  }

  // Print statistics even if no allocations eliminated - show deopt info
  log_info(gc)("Spark Escape Analysis Statistics:");
  log_info(gc)("  Optimization attempts: %zu", _optimization_attempts);
  log_info(gc)("  Deoptimizations: %zu", _deoptimization_count);

  if (_optimization_attempts > 0) {
    double deopt_rate = 100.0 * _deoptimization_count / _optimization_attempts;
    log_info(gc)("  Deoptimization rate: %.1f%%", deopt_rate);
    if (_adaptive_mode_disabled) {
      log_info(gc)("  Adaptive mode: DISABLED (high deopt rate)");
    }
  }

  if (_total_allocations_eliminated == 0) {
    log_info(gc)("  No allocations eliminated");
    return;
  }

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
