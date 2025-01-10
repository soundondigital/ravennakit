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
#include "ravennakit/core/math/sliding_window_average.hpp"
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
        offset_average_.add(measurement.offset_from_master);
        TRACY_PLOT("Offset from master (ms avg)", offset_average_.average() * 1000.0);

        measurements_.push_back(measurement);
        if (measurements_.size() > 1) {
            bool first = true;
            double local_interval = {};
            double master_interval = {};
            for (const auto& m : measurements_) {
                if (first) {
                    local_interval = m.sync_event_ingress_timestamp;
                    master_interval = m.corrected_master_event_timestamp;
                    first = false;
                    continue;
                }
                local_interval -= m.sync_event_ingress_timestamp;
                master_interval -= m.corrected_master_event_timestamp;
            }

            frequency_ratio_ = local_interval / master_interval;
            frequency_ratio_average_.add(frequency_ratio_);
        } else {
            frequency_ratio_ = 1.0;
        }

        TRACY_PLOT("Frequency ratio", frequency_ratio_);
        TRACY_PLOT("Frequency ratio (avg)", frequency_ratio_average_.average());
    }

    void step_clock(const double offset_from_master_seconds) {
        last_sync_ = system_clock_now();
        shift_ += -offset_from_master_seconds;
        TRACY_PLOT("Step clock add (ms)", shift_ * 1000.0);
        offset_average_.add(offset_from_master_seconds);
        frequency_ratio_average_.reset();
        frequency_ratio_ = 1.0;
        measurements_.clear();
    }

  private:
    ptp_timestamp last_sync_ = system_clock_now();
    double shift_ {};
    ring_buffer<ptp_measurement<double>> measurements_ {8};
    double frequency_ratio_ = 1.0;
    sliding_window_average frequency_ratio_average_ {100};
    sliding_window_average offset_average_ {100};
};

}  // namespace rav
