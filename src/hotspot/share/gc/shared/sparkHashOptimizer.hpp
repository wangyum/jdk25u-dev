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

#ifndef SHARE_GC_SHARED_SPARKHASHOPTIMIZER_HPP
#define SHARE_GC_SHARED_SPARKHASHOPTIMIZER_HPP

#include "memory/allStatic.hpp"
#include "utilities/globalDefinitions.hpp"

// Spark-specific hash operation optimization
//
// Apache Spark SQL relies heavily on hash operations:
// 1. Hash partitioning for shuffle operations
// 2. Hash aggregation (groupBy, agg, etc.)
// 3. Hash joins
// 4. UnsafeRow hash code computation
// 5. Murmur3 hashing for data distribution
//
// This optimizer provides infrastructure for optimizing these operations:
// - Pre-computation of hash codes for immutable objects
// - Caching of frequently computed hashes
// - Statistics tracking for hash operation patterns
// - Foundation for future JIT intrinsic optimizations

class SparkHashOptimizer : public AllStatic {
public:
  // Check if Spark hash optimizations are enabled
  static bool is_enabled() {
    return G1OptimizeForSpark && G1SparkOptimizeHashOperations;
  }

  // Initialize Spark hash optimizer
  // Called during VM initialization
  static void initialize();

  // Hash code caching for immutable objects
  // These would be used by future intrinsics or JIT optimizations
  static bool should_cache_hashcode(size_t object_size);
  static bool is_likely_immutable_spark_object(const char* class_name);

  // Statistics tracking
  static void record_hash_computation(const char* operation, size_t data_size);
  static void record_hash_cache_hit();
  static void record_hash_cache_miss();
  static void print_statistics();

  // Heuristics for hash optimization
  static bool is_partition_hash_operation();
  static bool is_aggregation_hash_operation();
  static bool is_join_hash_operation();

  // Logging
  static void log_optimization(const char* phase,
                               const char* operation,
                               const char* reason);

private:
  // Statistics
  static size_t _total_hash_computations;
  static size_t _partition_hash_count;
  static size_t _aggregation_hash_count;
  static size_t _join_hash_count;
  static size_t _cache_hits;
  static size_t _cache_misses;

  // Common Spark class patterns
  static bool is_unsafe_row_class(const char* class_name);
  static bool is_scala_tuple_class(const char* class_name);
  static bool is_spark_internal_class(const char* class_name);
};

#endif // SHARE_GC_SHARED_SPARKHASHOPTIMIZER_HPP
