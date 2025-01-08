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

#include "ptp_local_clock.hpp"
#include "ravennakit/core/tracy.hpp"
#include "types/ptp_timestamp.hpp"

#include <cstdint>

namespace rav {

/**
 * A class that maintains a local PTP clock as close as possible to some grand master clock.
 * This particular implementation maintains a 'virtual' clock based on the local clock and a correction value.
 */
class ptp_local_ptp_clock {
  public:
    ptp_local_ptp_clock() {
        // const auto steady_now =
        //     std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        //         .count();
        //
        // const auto system_clock_now =
        //     std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch())
        //         .count();
        //
        // correction_ns_ = static_cast<double>(steady_now) - static_cast<double>(system_clock_now);
    }

    [[nodiscard]] ptp_timestamp now() const {
        const auto now = ptp_local_clock::now();
        return now;
    }

    [[nodiscard]] bool is_calibrated() const {
        return false;  // TODO: Implement calibration
    }

    void adjust(const ptp_time_interval offset_from_master) {

        // if (offset_from_master == std::numeric_limits<int64_t>::max()) {
        //     const auto now = ptp_local_clock::now();
        //     RAV_ASSERT(now > best_guess_timestamp, "Expecting best now timestamp to be in the future");
        //     const auto guessed_correction = now - best_guess_timestamp;
        //     correction_ns_ = guessed_correction.to_time_interval_double();
        //     return;
        // }
        // if (offset_from_master == std::numeric_limits<int64_t>::min()) {
        //     const auto now = ptp_local_clock::now();
        //     RAV_ASSERT(best_guess_timestamp > now, "Expecting best guess timestamp to be in the future");
        //     const auto guessed_correction = best_guess_timestamp - now;
        //     correction_ns_ = guessed_correction.to_time_interval_double();
        //     return;
        // }
        // auto diff = -(offset_from_master / ptp_timestamp::k_time_interval_multiplier);
        // RAV_TRACE("Correct PTP clock by {} ns", diff);
        // TRACY_PLOT("Correction diff (ns): ", diff);
        // TRACY_PLOT("Correction value (ns): ", correction_ns_);
        // correction_ns_ = offset_from_master / ptp_timestamp::k_time_interval_multiplier;
    }

    void step_clock() {

    }

  private:
    ptp_time_interval correction_ns_ {};
};

}  // namespace rav
