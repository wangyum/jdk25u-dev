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

#include "precompiled.hpp"
#include "gc/shared/sparkTLABOptimizer.hpp"
#include "logging/log.hpp"
#include "runtime/javaThread.hpp"
#include "runtime/thread.hpp"
#include "gc/shared/tlab_globals.hpp"

bool SparkTLABOptimizer::is_spark_executor_thread(Thread* thread) {
  if (!thread->is_Java_thread()) {
    return false;
  }

  JavaThread* jt = JavaThread::cast(thread);
  const char* thread_name = jt->name();

  if (thread_name == nullptr) {
    return false;
  }

  // Detect Spark executor thread naming patterns:
  // - "Executor task launch worker for task X"
  // - "Executor task launch worker-X"
  // - Pattern: contains "Executor" and "task"
  return (strstr(thread_name, "Executor") != nullptr &&
          strstr(thread_name, "task") != nullptr) ||
         strstr(thread_name, "executor") != nullptr;
}

bool SparkTLABOptimizer::is_spark_driver_thread(Thread* thread) {
  if (!thread->is_Java_thread()) {
    return false;
  }

  JavaThread* jt = JavaThread::cast(thread);
  const char* thread_name = jt->name();

  if (thread_name == nullptr) {
    return false;
  }

  // Detect Spark driver threads
  return strstr(thread_name, "SparkSubmit") != nullptr ||
         strstr(thread_name, "driver") != nullptr ||
         strstr(thread_name, "DAGScheduler") != nullptr;
}

bool SparkTLABOptimizer::is_high_allocation_rate(double allocation_fraction,
                                                  size_t tlab_capacity) {
  if (tlab_capacity == 0) {
    return false;
  }

  // Calculate allocation rate as percentage of TLAB capacity
  // High rate = allocation_fraction indicates heavy TLAB usage
  double rate_percent = allocation_fraction * 100.0;

  return rate_percent >= SparkTLABHighAllocThreshold;
}

uintx SparkTLABOptimizer::get_thread_tlab_multiplier(Thread* thread) {
  if (!is_thread_detection_enabled()) {
    return 1; // No boost if thread detection disabled
  }

  if (is_spark_executor_thread(thread)) {
    // Executor threads get the largest boost
    return SparkExecutorTLABMultiplier;
  } else if (is_spark_driver_thread(thread)) {
    // Driver threads get a moderate boost
    return MAX2((uintx)2, SparkExecutorTLABMultiplier / 2);
  }

  return 1; // No boost for other threads
}

size_t SparkTLABOptimizer::calculate_tlab_size(Thread* thread,
                                                size_t base_size,
                                                double allocation_fraction,
                                                size_t tlab_capacity) {
  if (!is_enabled()) {
    return base_size;
  }

  size_t optimized_size = base_size;

  // Step 1: Apply thread-based multiplier
  uintx thread_multiplier = get_thread_tlab_multiplier(thread);
  if (thread_multiplier > 1) {
    optimized_size *= thread_multiplier;
    log_trace(gc, tlab)("Spark TLAB: Thread multiplier %ux applied for %s",
                        thread_multiplier,
                        thread->is_Java_thread() ?
                        JavaThread::cast(thread)->name() : "unknown");
  }

  // Step 2: Boost for high allocation rate
  if (is_high_allocation_rate(allocation_fraction, tlab_capacity)) {
    size_t boost = (optimized_size * SparkTLABSizeBoostPercent) / 100;
    size_t boosted_size = MAX2(optimized_size, boost);

    log_trace(gc, tlab)("Spark TLAB: High allocation rate detected (%.2f%%), "
                        "boosting size from " SIZE_FORMAT " to " SIZE_FORMAT,
                        allocation_fraction * 100.0,
                        optimized_size,
                        boosted_size);

    optimized_size = boosted_size;
  }

  return optimized_size;
}

size_t SparkTLABOptimizer::get_refill_waste_limit(size_t desired_size) {
  if (!is_enabled() || !SparkTLABReduceRefillWaste) {
    // Use standard calculation
    return desired_size / TLABRefillWasteFraction;
  }

  // Use Spark-optimized refill waste fraction
  // Lower fraction = keep TLABs longer = less refill overhead
  return desired_size / SparkTLABRefillWasteFraction;
}

size_t SparkTLABOptimizer::adjust_for_spark_patterns(Thread* thread,
                                                      size_t computed_size) {
  if (!is_enabled()) {
    return computed_size;
  }

  // Additional pattern-based adjustments
  // For now, just return the computed size
  // Future: add more sophisticated pattern detection

  return computed_size;
}

void SparkTLABOptimizer::log_optimization(Thread* thread,
                                          size_t original_size,
                                          size_t optimized_size,
                                          const char* reason) {
  if (log_is_enabled(Debug, gc, tlab)) {
    const char* thread_name = thread->is_Java_thread() ?
                              JavaThread::cast(thread)->name() : "unknown";

    log_debug(gc, tlab)("Spark TLAB optimization: thread=%s, "
                        "original=" SIZE_FORMAT ", optimized=" SIZE_FORMAT ", "
                        "reason=%s",
                        thread_name, original_size, optimized_size, reason);
  }
}
