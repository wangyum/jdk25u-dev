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

#ifndef SHARE_GC_SHARED_SPARKTLABOPTIMIZER_HPP
#define SHARE_GC_SHARED_SPARKTLABOPTIMIZER_HPP

#include "gc/shared/tlab_globals.hpp"
#include "memory/allocation.hpp"
#include "runtime/thread.hpp"
#include "utilities/globalDefinitions.hpp"

// SparkTLABOptimizer: Adaptive TLAB sizing for Apache Spark workloads
//
// Spark has distinct allocation patterns:
// - High per-thread allocation rate during task processing
// - Bursty allocation during map/shuffle/reduce phases
// - Many executor threads allocating simultaneously
// - Short-lived objects that die in young generation
//
// This optimizer:
// 1. Detects Spark executor threads by name pattern
// 2. Monitors allocation rate and increases TLAB size for high rates
// 3. Reduces TLAB refill waste for bursty patterns
// 4. Provides dynamic TLAB sizing based on workload phase
class SparkTLABOptimizer : public AllStatic {
private:
  // High water mark for allocation rate (as percentage of capacity)
  static const uintx HIGH_ALLOC_THRESHOLD = 80;

  // Detect if a thread is a Spark executor thread
  static bool is_spark_executor_thread(Thread* thread);

  // Detect if a thread is a Spark driver thread
  static bool is_spark_driver_thread(Thread* thread);

  // Detect if current workload has high allocation rate
  static bool is_high_allocation_rate(double allocation_fraction, size_t tlab_capacity);

  // Get the TLAB size multiplier for a thread
  static uintx get_thread_tlab_multiplier(Thread* thread);

public:
  // Check if Spark adaptive TLAB is enabled
  static bool is_enabled();

  // Check if thread detection is enabled
  static bool is_thread_detection_enabled();

  // Calculate optimal TLAB size for a thread
  static size_t calculate_tlab_size(Thread* thread,
                                     size_t base_size,
                                     double allocation_fraction,
                                     size_t tlab_capacity);

  // Get refill waste limit for Spark workloads
  static size_t get_refill_waste_limit(size_t desired_size);

  // Adjust TLAB size based on Spark patterns
  static size_t adjust_for_spark_patterns(Thread* thread, size_t computed_size);

  // Log TLAB optimization decisions
  static void log_optimization(Thread* thread,
                                size_t original_size,
                                size_t optimized_size,
                                const char* reason);
};

#endif // SHARE_GC_SHARED_SPARKTLABOPTIMIZER_HPP
