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
#include "detail/ptp_basic_filter.hpp"
#include "detail/ptp_measurement.hpp"
#include "ravennakit/core/util/tracy.hpp"
#include "ravennakit/core/util.hpp"
#include "ravennakit/core/chrono/high_resolution_clock.hpp"
#include "ravennakit/core/math/running_average.hpp"
#include "ravennakit/core/math/sliding_stats.hpp"
#include "ravennakit/core/util/throttle.hpp"
#include "types/ptp_timestamp.hpp"

#include <cstdint>

namespace rav::ptp {

/**
 * A class that maintains a local PTP clock as close as possible to some grand master clock.
 * This particular implementation maintains a 'virtual' clock based on the monotonic system clock.
 */
class LocalPtpClock {
  public:
    explicit LocalPtpClock(LocalClock& local_clock) : local_clock_(local_clock) {}

    /**
     * @return The best estimate of 'now' in the timescale of the grand master clock.
     */
    [[nodiscard]] Timestamp now() const {
        TRACY_ZONE_SCOPED;
        return local_clock_.now();
    }

    /**
     * @return True if the clock is locked and within a certain amount of error, false otherwise.
     */
    [[nodiscard]] bool is_calibrated() const {
        TRACY_ZONE_SCOPED;
        return local_clock_.is_calibrated();
    }

    /**
     * @return True when the clock is locked, false otherwise. A clock is considered locked when it has received enough
     * adjustments. When a clock steps, the adjustments are reset.
     */
    [[nodiscard]] bool is_locked() const {
        TRACY_ZONE_SCOPED;
        return local_clock_.is_locked();
    }

    /**
     * Adjusts the speed of the clock based on the given measurement.
     * @param measurement The measurement to adjust the clock with.
     */
    void adjust(const Measurement<double>& measurement) {
        TRACY_ZONE_SCOPED;

        // Add the measurement to the offset statistics in any case to allow the outlier filtering to dynamically
        // adjust.
        offset_stats_.add(measurement.offset_from_master);

        // Filter out outliers, allowing a maximum per non-filtered outliers to avoid getting in a loop where all
        // measurements are filtered out and no adjustment is made anymore.
        if (is_calibrated() && offset_stats_.is_outlier_zscore(measurement.offset_from_master, 1.75)) {
            ignored_outliers_++;
            TRACY_PLOT("Offset from master outliers", measurement.offset_from_master * 1000.0);
            TRACY_MESSAGE("Ignoring outlier in offset from master");
            return;
        }

        filtered_offset_stats_.add(measurement.offset_from_master);
        local_clock_.adjust(measurement.offset_from_master);

        // Note (Ruurd): I wonder whether we should move this to the local clock, based on the actual (non-median)
        // offset.
        local_clock_.set_calibrated(is_between(offset_stats_.median(), -k_calibrated_threshold, k_calibrated_threshold)
        );

        TRACY_PLOT("Offset from master median (ms)", offset_stats_.median() * 1000.0);
        TRACY_PLOT("Offset from master outliers", 0.0);
        TRACY_PLOT("Filtered offset from master (ms)", measurement.offset_from_master * 1000.0);
        TRACY_PLOT("Filtered offset from master median (ms)", filtered_offset_stats_.median() * 1000.0);
        TRACY_PLOT("Frequency ratio", local_clock_.get_frequency_ratio());

        if (trace_adjustments_throttle_.update()) {
            RAV_TRACE(
                "Clock stats: offset_from_master=[min={}, max={}], ratio={}, ignored_outliers={}",
                filtered_offset_stats_.min() * 1000.0, filtered_offset_stats_.max() * 1000.0,
                local_clock_.get_frequency_ratio(), ignored_outliers_
            );
            ignored_outliers_ = 0;
        }
    }

    /**
     * Steps the clock to the given offset from the master clock.
     * @param offset_from_master_seconds The offset from the master clock in seconds.
     */
    void step_clock(const double offset_from_master_seconds) {
        TRACY_ZONE_SCOPED;

        local_clock_.step(offset_from_master_seconds);
        local_clock_.set_calibrated(false);
        offset_stats_.reset();

        RAV_TRACE("Stepping clock: offset_from_master={}", offset_from_master_seconds);
    }

    /**
     * Updates the local clock based on the given measurement. Depending on the offset from the master, the clock will
     * either step or adjust.
     * @param measurement The measurement to update the clock with.
     * @return True if the clock stepped, false otherwise.
     */
    bool update(const Measurement<double>& measurement) {
        TRACY_ZONE_SCOPED;

        TRACY_PLOT("Offset from master (ms)", measurement.offset_from_master * 1000.0);

        if (std::fabs(measurement.offset_from_master) >= k_clock_step_threshold_seconds) {
            step_clock(measurement.offset_from_master);
            return true;
        }
        adjust(measurement);
        return false;
    }

  private:
    constexpr static double k_calibrated_threshold = 0.0018;
    constexpr static int64_t k_clock_step_threshold_seconds = 1;

    LocalClock& local_clock_;

    SlidingStats offset_stats_ {51};
    SlidingStats filtered_offset_stats_ {51};
    uint32_t ignored_outliers_ = 0;
    Throttle<void> trace_adjustments_throttle_ {std::chrono::seconds(5)};
};

}  // namespace rav::ptp
