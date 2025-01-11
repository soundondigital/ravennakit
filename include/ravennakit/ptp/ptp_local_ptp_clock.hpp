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

#include "detail/ptp_measurement.hpp"
#include "ravennakit/core/tracy.hpp"
#include "ravennakit/core/util.hpp"
#include "ravennakit/core/chrono/high_resolution_clock.hpp"
#include "ravennakit/core/math/running_average.hpp"
#include "ravennakit/core/math/sliding_average.hpp"
#include "ravennakit/core/math/sliding_median.hpp"
#include "types/ptp_timestamp.hpp"

#include <cstdint>

namespace rav {

/**
 * A class that maintains a local PTP clock as close as possible to some grand master clock.
 * This particular implementation maintains a 'virtual' clock based on the local clock and a correction value.
 */
class ptp_local_ptp_clock {
  public:
    ptp_local_ptp_clock() {}

    static ptp_timestamp system_clock_now() {
        return ptp_timestamp(high_resolution_clock::now());
    }

    [[nodiscard]] ptp_timestamp now() const {
        const auto system_now = system_clock_now();
        const auto elapsed = (system_now - last_sync_).total_seconds_double();
        auto result = last_sync_;
        result.add_seconds(elapsed * frequency_ratio_);
        result.add_seconds(shift_);
        return result;
    }

    [[nodiscard]] bool is_calibrated() const {
        return false;  // TODO: Implement calibration
    }

    void adjust(const ptp_measurement<double>& measurement) {
        const auto system_now = system_clock_now();
        shift_ = now().total_seconds_double() - system_now.total_seconds_double();
        last_sync_ = system_now;

        offset_average_.add(measurement.offset_from_master);
        TRACY_PLOT("Offset from master (ms median)", offset_average_.median() * 1000.0);
        TRACY_PLOT("Adjustments since last step", static_cast<int64_t>(adjustments_since_last_step_));

        if (adjustments_since_last_step_ >= 10) {
            constexpr double base = 1.5;  // The higher the value, the faster the clock will adjust (>= 1.0)
            constexpr double max_ratio = 0.5; // +/-
            constexpr double max_step = 0.001;  // Maximum step size
            const auto nominal_ratio =
                std::clamp(std::pow(base, -measurement.offset_from_master), 1.0 - max_ratio, 1 + max_ratio);

            if (std::fabs(nominal_ratio - frequency_ratio_) > max_step) {
                if (frequency_ratio_ < nominal_ratio) {
                    frequency_ratio_ += max_step;
                } else {
                    frequency_ratio_ -= max_step;
                }
            } else {
                frequency_ratio_ = nominal_ratio;
            }
        } else {
            adjustments_since_last_step_++;
            frequency_ratio_ = 1.0;
        }

        TRACY_PLOT("Frequency ratio", frequency_ratio_);
    }

    void step_clock(const double offset_from_master_seconds) {
        last_sync_ = system_clock_now();
        shift_ += -offset_from_master_seconds;
        TRACY_PLOT("Step clock add (ms)", shift_ * 1000.0);
        // offset_average_.add(offset_from_master_seconds);
        offset_average_.reset();
        frequency_ratio_ = 1.0;
        adjustments_since_last_step_ = 0;
    }

  private:
    ptp_timestamp last_sync_ = system_clock_now();
    double shift_ {};
    double frequency_ratio_ = 1.0;
    sliding_median offset_average_ {101};
    size_t adjustments_since_last_step_ {};
};

}  // namespace rav
