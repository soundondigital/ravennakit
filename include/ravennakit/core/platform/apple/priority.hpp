/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/core/platform.hpp"

#if RAV_APPLE

    #include <mach/mach_types.h>
    #include <mach/thread_act.h>

namespace rav {

inline bool set_thread_realtime(const int period_ms, const int computation_ms, const int constraint_ms) {
    thread_time_constraint_policy time_constraint_policy{};
    const thread_port_t thread_port = pthread_mach_thread_np(pthread_self());

    // https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/scheduler/scheduler.html
    // https://developer.apple.com/library/archive/qa/qa1398/_index.html

    time_constraint_policy.period = period_ms;            // HZ/160
    time_constraint_policy.computation = computation_ms;  // HZ/3300;
    time_constraint_policy.constraint = constraint_ms;    // HZ/2200;
    time_constraint_policy.preemptible = 1;

    const int result = thread_policy_set(
        thread_port, THREAD_TIME_CONSTRAINT_POLICY, reinterpret_cast<thread_policy_t>(&time_constraint_policy),
        THREAD_TIME_CONSTRAINT_POLICY_COUNT
    );

    if (result != KERN_SUCCESS) {
        return false;
    }

    return true;
}

}  // namespace rav

#endif
