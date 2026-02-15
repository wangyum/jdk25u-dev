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
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_GC_SHARED_SPARKVECTORIZATIONOPTIMIZER_HPP
#define SHARE_GC_SHARED_SPARKVECTORIZATIONOPTIMIZER_HPP

#include "memory/allStatic.hpp"
#include "runtime/globals.hpp"
#include "utilities/globalDefinitions.hpp"

// Spark-specific vectorization optimization
//
// Apache Spark SQL extensively uses vectorized execution with columnar storage:
// 1. ColumnVector (OnHeapColumnVector, OffHeapColumnVector, ArrowColumnVector)
// 2. ColumnarBatch - batch of rows in columnar format (default 4096 rows)
// 3. Batch operations on arrays (filters, projections, aggregations)
// 4. Dictionary encoding for string columns
//
// Vectorization opportunities:
// - Loop unrolling for batch operations
// - SIMD instructions for arithmetic/comparison
// - Prefetching for sequential column access
// - Bounds check elimination in safe contexts
// - Auto-vectorization hints to C2 compiler
//
// This optimizer provides:
// 1. Detection of vectorizable Spark operations
// 2. Hints to JIT compiler for SIMD optimization
// 3. Prefetch distance tuning for columnar access
// 4. Bounds check elimination in controlled loops
// 5. Statistics on vectorization effectiveness

class SparkVectorizationOptimizer : public AllStatic {
private:
  // Statistics
  static size_t _total_vectorized_operations;
  static size_t _total_elements_processed;
  static size_t _bounds_checks_eliminated;
  static size_t _auto_vectorization_applied;
  static size_t _prefetch_hints_applied;

  // Operation type counters
  static size_t _column_vector_operations;
  static size_t _batch_operations;
  static size_t _dictionary_decodes;

  // Performance tracking
  static size_t _bytes_processed;

  // Pattern matching helpers
  static bool matches_pattern(const char* str, const char* pattern);
  static bool is_spark_sql_package(const char* class_name);
  static bool is_column_vector_class(const char* class_name);
  static bool is_columnar_batch_class(const char* class_name);

public:
  // Check if Spark vectorization optimizations are enabled
  static bool is_enabled();

  // Initialize vectorization optimizer
  static void initialize();

  // Detect if operation is vectorizable
  static bool is_vectorizable_operation(const char* operation_type,
                                        size_t elements_count);

  // Check if bounds checks can be safely eliminated
  static bool can_eliminate_bounds_checks(const char* loop_context,
                                          size_t array_length);

  // Suggest prefetch distance for column access
  static size_t get_prefetch_distance(size_t column_width);

  // Check if auto-vectorization should be applied
  static bool should_auto_vectorize(const char* loop_type,
                                    size_t iteration_count);

  // Record vectorized operation
  static void record_vectorized_operation(const char* operation_type,
                                          size_t elements_processed);

  // Record bounds check elimination
  static void record_bounds_check_eliminated(size_t count);

  // Record auto-vectorization application
  static void record_auto_vectorization(const char* loop_name,
                                        size_t vector_width);

  // Statistics and diagnostics
  static void print_statistics();
  static void log_optimization(const char* phase,
                               const char* class_name,
                               const char* reason);
};

#endif // SHARE_GC_SHARED_SPARKVECTORIZATIONOPTIMIZER_HPP
