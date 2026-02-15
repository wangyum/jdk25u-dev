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

#ifndef SHARE_GC_SHARED_SPARKESCAPEANALYSISOPTIMIZER_HPP
#define SHARE_GC_SHARED_SPARKESCAPEANALYSISOPTIMIZER_HPP

#include "memory/allStatic.hpp"
#include "runtime/globals.hpp"
#include "utilities/globalDefinitions.hpp"

// Spark-specific escape analysis optimization
//
// Apache Spark SQL creates massive amounts of short-lived objects:
// 1. InternalRow instances (GenericInternalRow, SpecificInternalRow)
// 2. Iterator wrapper objects
// 3. Temporary aggregation state objects
// 4. Columnar batch processing objects
// 5. Expression evaluation temporary objects
//
// Many of these objects don't escape their allocation scope:
// - Created in a method
// - Used locally
// - Never stored in fields or returned
// - Ideal candidates for stack allocation
//
// This optimizer enhances JVM escape analysis for Spark patterns:
// - Identifies Spark-specific non-escaping patterns
// - Provides hints to C2 compiler for aggressive optimization
// - Tracks statistics on allocation elimination opportunities
// - Foundation for scalar replacement and stack allocation

class SparkEscapeAnalysisOptimizer : public AllStatic {
public:
  // Check if Spark escape analysis optimizations are enabled
  static bool is_enabled();

  // Initialize Spark escape analysis optimizer
  // Called during VM initialization
  static void initialize();

  // Class pattern detection for escape analysis
  // These patterns are common non-escaping objects in Spark
  static bool is_internal_row_class(const char* class_name);
  static bool is_iterator_wrapper_class(const char* class_name);
  static bool is_expression_eval_class(const char* class_name);
  static bool is_aggregation_buffer_class(const char* class_name);
  static bool is_columnar_batch_class(const char* class_name);

  // Heuristics for escape analysis
  // Determine if object is likely non-escaping
  static bool is_likely_non_escaping_spark_object(const char* class_name,
                                                   size_t object_size);

  // Should this object be aggressively scalarized?
  // Scalar replacement: replace object with individual fields
  static bool should_scalarize_aggressively(const char* class_name,
                                            size_t field_count);

  // Should this allocation be attempted on stack?
  // Stack allocation for objects that don't escape
  static bool should_attempt_stack_allocation(const char* class_name,
                                              size_t object_size);

  // Statistics tracking
  static void record_allocation_eliminated(const char* class_name, size_t size);
  static void record_scalar_replacement(const char* class_name, size_t field_count);
  static void record_stack_allocation(const char* class_name, size_t size);
  static void print_statistics();

  // Logging
  static void log_optimization(const char* phase,
                               const char* class_name,
                               const char* reason);

private:
  // Statistics
  static size_t _total_allocations_eliminated;
  static size_t _scalar_replacements;
  static size_t _stack_allocations;
  static size_t _internal_row_eliminations;
  static size_t _iterator_eliminations;
  static size_t _expression_eliminations;
  static size_t _bytes_saved;

  // Pattern matching helpers
  static bool matches_pattern(const char* str, const char* pattern);
  static bool is_spark_sql_package(const char* class_name);
};

#endif // SHARE_GC_SHARED_SPARKESCAPEANALYSISOPTIMIZER_HPP
