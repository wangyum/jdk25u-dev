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

#include "gc/shared/sparkHashOptimizer.hpp"
#include "logging/log.hpp"
#include "runtime/globals.hpp"
#include "utilities/globalDefinitions.hpp"
#include <cstring>

// Statistics
size_t SparkHashOptimizer::_total_hash_computations = 0;
size_t SparkHashOptimizer::_partition_hash_count = 0;
size_t SparkHashOptimizer::_aggregation_hash_count = 0;
size_t SparkHashOptimizer::_join_hash_count = 0;
size_t SparkHashOptimizer::_cache_hits = 0;
size_t SparkHashOptimizer::_cache_misses = 0;

void SparkHashOptimizer::initialize() {
  if (!is_enabled()) {
    return;
  }

  log_info(gc, init)("Spark Hash Operation Optimization enabled");
  log_info(gc, init)("  Hash Caching: %s",
                     G1SparkEnableHashCaching ? "enabled" : "disabled");
  log_info(gc, init)("  UnsafeRow Optimization: %s",
                     G1SparkOptimizeUnsafeRowHash ? "enabled" : "disabled");

  // Reset statistics
  _total_hash_computations = 0;
  _partition_hash_count = 0;
  _aggregation_hash_count = 0;
  _join_hash_count = 0;
  _cache_hits = 0;
  _cache_misses = 0;

  log_debug(gc)("Spark Hash Optimizer initialized");
}

bool SparkHashOptimizer::should_cache_hashcode(size_t object_size) {
  if (!is_enabled() || !G1SparkEnableHashCaching) {
    return false;
  }

  // Cache hash codes for objects in typical Spark range
  // UnsafeRow objects are typically 100-10000 bytes
  // Smaller objects: not worth the caching overhead
  // Larger objects: less likely to be reused
  return object_size >= 64 && object_size <= 16384;
}

bool SparkHashOptimizer::is_unsafe_row_class(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match: org/apache/spark/sql/catalyst/expressions/UnsafeRow
  return strstr(class_name, "UnsafeRow") != nullptr &&
         strstr(class_name, "spark") != nullptr;
}

bool SparkHashOptimizer::is_scala_tuple_class(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match: scala/Tuple2, scala/Tuple3, etc.
  return strstr(class_name, "scala") != nullptr &&
         strstr(class_name, "Tuple") != nullptr;
}

bool SparkHashOptimizer::is_spark_internal_class(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // Match various Spark internal classes
  return strstr(class_name, "org/apache/spark") != nullptr &&
         (strstr(class_name, "InternalRow") != nullptr ||
          strstr(class_name, "HashPartitioner") != nullptr ||
          strstr(class_name, "HashAggregator") != nullptr ||
          strstr(class_name, "HashJoin") != nullptr);
}

bool SparkHashOptimizer::is_likely_immutable_spark_object(const char* class_name) {
  if (class_name == nullptr) {
    return false;
  }

  // UnsafeRow is mutable, but often used immutably after creation
  if (is_unsafe_row_class(class_name)) {
    return true;
  }

  // Scala tuples are immutable
  if (is_scala_tuple_class(class_name)) {
    return true;
  }

  // Other Spark SQL immutable classes
  if (strstr(class_name, "UTF8String") != nullptr ||
      strstr(class_name, "GenericInternalRow") != nullptr) {
    return true;
  }

  return false;
}

void SparkHashOptimizer::record_hash_computation(const char* operation, size_t data_size) {
  if (!is_enabled()) {
    return;
  }

  _total_hash_computations++;

  // Classify hash operation type based on operation string
  if (operation != nullptr) {
    if (strstr(operation, "partition") != nullptr) {
      _partition_hash_count++;
    } else if (strstr(operation, "aggregate") != nullptr ||
               strstr(operation, "group") != nullptr) {
      _aggregation_hash_count++;
    } else if (strstr(operation, "join") != nullptr) {
      _join_hash_count++;
    }
  }

  log_trace(gc)("Spark Hash: %s, size=%zu", operation, data_size);
}

void SparkHashOptimizer::record_hash_cache_hit() {
  _cache_hits++;
}

void SparkHashOptimizer::record_hash_cache_miss() {
  _cache_misses++;
}

void SparkHashOptimizer::print_statistics() {
  if (!is_enabled()) {
    return;
  }

  if (_total_hash_computations == 0) {
    return;
  }

  double partition_percent = 100.0 * _partition_hash_count / _total_hash_computations;
  double aggregation_percent = 100.0 * _aggregation_hash_count / _total_hash_computations;
  double join_percent = 100.0 * _join_hash_count / _total_hash_computations;

  log_info(gc)("Spark Hash Optimizer Statistics:");
  log_info(gc)("  Total hash computations: %zu", _total_hash_computations);
  log_info(gc)("  Partition hashes: %zu (%.1f%%)",
               _partition_hash_count, partition_percent);
  log_info(gc)("  Aggregation hashes: %zu (%.1f%%)",
               _aggregation_hash_count, aggregation_percent);
  log_info(gc)("  Join hashes: %zu (%.1f%%)",
               _join_hash_count, join_percent);

  if (G1SparkEnableHashCaching) {
    size_t total_cache_ops = _cache_hits + _cache_misses;
    if (total_cache_ops > 0) {
      double hit_rate = 100.0 * _cache_hits / total_cache_ops;
      log_info(gc)("  Cache hits: %zu, misses: %zu (%.1f%% hit rate)",
                   _cache_hits, _cache_misses, hit_rate);
    }
  }
}

bool SparkHashOptimizer::is_partition_hash_operation() {
  // This would be called from JIT-compiled code in full intrinsic implementation
  // For now, just a placeholder
  return false;
}

bool SparkHashOptimizer::is_aggregation_hash_operation() {
  // This would be called from JIT-compiled code in full intrinsic implementation
  // For now, just a placeholder
  return false;
}

bool SparkHashOptimizer::is_join_hash_operation() {
  // This would be called from JIT-compiled code in full intrinsic implementation
  // For now, just a placeholder
  return false;
}

void SparkHashOptimizer::log_optimization(const char* phase,
                                          const char* operation,
                                          const char* reason) {
  if (log_is_enabled(Debug, gc)) {
    log_debug(gc)("Spark Hash optimization: phase=%s, operation=%s, reason=%s",
                  phase, operation, reason);
  }
}
