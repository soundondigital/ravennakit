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
        auto system_now = system_clock_now();
        const auto elapsed = system_now.total_seconds_double() - last_sync_.total_seconds_double();
        const auto correction = elapsed * frequency_correction_ppm_ / 1'000'000'000;
        system_now.add_seconds(shift_ + correction);
        return system_now;
    }

    [[nodiscard]] bool is_calibrated() const {
        return false;  // TODO: Implement calibration
    }

    void adjust(const ptp_measurement<double>& measurement) {
        const auto system_now = system_clock_now();
        const auto ptp_now = now();
        shift_ = (ptp_now - system_now).total_seconds_double();
        last_sync_ = system_now;

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

            const auto ratio = local_interval / master_interval;
            const auto ppm = -(ratio - 1.0) * 1e6;
            freq_average_.add(ppm);
            TRACY_PLOT("Frequency ratio (ppm)", ppm);
            TRACY_PLOT("Frequency ratio (ppm, avg)", freq_average_.average());
            frequency_correction_ppm_ += ppm;  // TODO: Take the direct ppm?
        } else {
            frequency_correction_ppm_ = {};
        }
    }

    void step_clock(const double offset_from_master_seconds) {
        last_sync_ = system_clock_now();
        const auto multiplier = 1'000'000.0 + frequency_correction_ppm_;
        const auto reciprocal = 1'000'000.0 / multiplier;
        const auto diff = -offset_from_master_seconds * reciprocal;
        shift_ += diff;
        TRACY_PLOT("Step clock (ms)", diff * 1000.0);
        freq_average_.reset();
        freq_window_average_.reset();
    }

  private:
    ptp_timestamp last_sync_ = system_clock_now();    // Timestamp from ptp_local_clock when the clock was last synchronized
    double shift_ {};  // Seconds
    ring_buffer<ptp_measurement<double>> measurements_ {8};
    double frequency_correction_ppm_ = {};
    running_average freq_average_;
    sliding_window_average freq_window_average_ {100};
};

}  // namespace rav
