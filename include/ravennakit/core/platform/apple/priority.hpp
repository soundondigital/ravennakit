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

[[nodiscard]] inline bool
set_thread_realtime(const uint64_t period_ns, const uint64_t computation_ns, const uint64_t constraint_ns) {
    if (constraint_ns < computation_ns) {
        return false;
    }

    thread_time_constraint_policy time_constraint_policy {};
    const thread_port_t thread_port = pthread_mach_thread_np(pthread_self());

    // https://developer.apple.com/library/archive/documentation/Darwin/Conceptual/KernelProgramming/scheduler/scheduler.html
    // https://developer.apple.com/library/archive/qa/qa1398/_index.html

    time_constraint_policy.period = static_cast<uint32_t>(mach_nanoseconds_to_absolute_time(period_ns));
    time_constraint_policy.computation = static_cast<uint32_t>(mach_nanoseconds_to_absolute_time(computation_ns));
    time_constraint_policy.constraint = static_cast<uint32_t>(mach_nanoseconds_to_absolute_time(constraint_ns));
    time_constraint_policy.preemptible = 1;

    int result = thread_policy_set(
        thread_port, THREAD_TIME_CONSTRAINT_POLICY, reinterpret_cast<thread_policy_t>(&time_constraint_policy),
        THREAD_TIME_CONSTRAINT_POLICY_COUNT
    );

    if (result == KERN_INVALID_ARGUMENT && computation_ns > 50 * 1000 * 1000) {
        // Taken from JUCE: testing has shown that passing a computation value > 50ms can lead to thread_policy_set
        // returning an error indicating that an invalid argument was passed. If that happens this code tries to limit
        // that value in the hope of resolving the issue.
        time_constraint_policy.computation = 50 * 1000 * 1000;
        result = thread_policy_set(
            thread_port, THREAD_TIME_CONSTRAINT_POLICY, reinterpret_cast<thread_policy_t>(&time_constraint_policy),
            THREAD_TIME_CONSTRAINT_POLICY_COUNT
        );
    }

    if (result != KERN_SUCCESS) {
        return false;
    }

    return true;
}

}  // namespace rav

#endif
