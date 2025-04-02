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

#include "detail/ptp_basic_filter.hpp"
#include "detail/ptp_measurement.hpp"
#include "ravennakit/core/tracy.hpp"
#include "ravennakit/core/util.hpp"
#include "ravennakit/core/chrono/high_resolution_clock.hpp"
#include "ravennakit/core/math/running_average.hpp"
#include "ravennakit/core/math/sliding_stats.hpp"
#include "ravennakit/core/util/throttle.hpp"
#include "types/ptp_timestamp.hpp"

#include <cstdint>

namespace rav::ptp {

class LocalSystemClock {
  public:
    /**
     * @return The best estimate of 'now' in the timescale of the grand master clock.
     */
    [[nodiscard]] Timestamp now() const {
        return get_ptp_time(system_monotonic_now());
    }

    /**
     * @param system_time The local timestamp to convert to the timescale of the grand master clock.
     * @return The best estimate of the PTP time based on given system time.
     */
    [[nodiscard]] Timestamp get_ptp_time(const Timestamp system_time) const {
        TRACY_ZONE_SCOPED;
        const auto elapsed = system_time.total_seconds_double() - last_sync_.total_seconds_double();
        auto result = last_sync_;
        result.add_seconds(elapsed * frequency_ratio_);
        result.add_seconds(shift_);
        return result;
    }

    /**
     * Adjusts the correction of this clock by adding the given shift and frequency ratio.
     * @param offset_from_master The shift to apply to the clock.
     */
    void adjust(const double offset_from_master) {
        TRACY_ZONE_SCOPED;
        last_sync_ = system_monotonic_now();
        shift_ += -offset_from_master;

        constexpr double max_ratio = 0.5;  // +/-
        const auto nominal_ratio = 0.001 * std::pow(-offset_from_master, 3) + 1.0;
        frequency_ratio_ = std::clamp(nominal_ratio, 1.0 - max_ratio, 1 + max_ratio);
    }

    /**
     * Steps the clock to the given offset from the master clock. This is used when the clock is out of sync and needs
     * to be reset.
     * @param offset_from_master The offset from the master clock in seconds.
     */
    void step(const double offset_from_master) {
        TRACY_ZONE_SCOPED;
        last_sync_ = system_monotonic_now();
        shift_ = -offset_from_master;
        frequency_ratio_ = 1.0;
    }

    /**
     * @return The current shift of the clock.
     */
    [[nodiscard]] double get_frequency_ratio() const {
        return frequency_ratio_;
    }

  private:
    Timestamp last_sync_ = system_monotonic_now();
    double shift_ {};
    double frequency_ratio_ = 1.0;

    /**
     * @return The current system time as a PTP timestamp. The timestamp is based on the high resolution clock and bears
     * no relation to wallclock time (UTC or TAI).
     */
    static Timestamp system_monotonic_now() {
        return Timestamp(HighResolutionClock::now());
    }
};

/**
 * A class that maintains a local PTP clock as close as possible to some grand master clock.
 * This particular implementation maintains a 'virtual' clock based on the monotonic system clock.
 */
class LocalPtpClock {
  public:
    explicit LocalPtpClock(LocalSystemClock& local_clock) : local_clock_(local_clock) {}

    /**
     * @param local_time The local timestamp to convert to the timescale of the grand master clock.
     * @return The best estimate of the PTP time based on given system time.
     */
    [[nodiscard]] Timestamp system_to_ptp_time(const Timestamp local_time) const {
        TRACY_ZONE_SCOPED;
        return local_clock_.get_ptp_time(local_time);
    }

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
        return is_locked() && is_between(offset_stats_.median(), -k_calibrated_threshold, k_calibrated_threshold);
    }

    /**
     * @return True when the clock is locked, false otherwise. A clock is considered locked when it has received enough
     * adjustments. When a clock steps, the adjustments are reset.
     */
    [[nodiscard]] bool is_locked() const {
        TRACY_ZONE_SCOPED;
        return adjustments_since_last_step_ >= k_lock_threshold;
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
            RAV_WARNING("Ignoring outlier in offset from master: {}", measurement.offset_from_master * 1000.0);
            TRACY_PLOT("Offset from master outliers", measurement.offset_from_master * 1000.0);
            TRACY_MESSAGE("Ignoring outlier in offset from master");
            return;
        }

        filtered_offset_stats_.add(measurement.offset_from_master);

        local_clock_.adjust(measurement.offset_from_master);
        adjustments_since_last_step_++;

        TRACY_PLOT("Offset from master median (ms)", offset_stats_.median() * 1000.0);
        TRACY_PLOT("Offset from master outliers", 0.0);
        TRACY_PLOT("Filtered offset from master (ms)", measurement.offset_from_master * 1000.0);
        TRACY_PLOT("Filtered offset from master median (ms)", filtered_offset_stats_.median() * 1000.0);
        TRACY_PLOT("Frequency ratio", local_clock_.get_frequency_ratio());

        if (trace_adjustments_throttle_.update()) {
            RAV_TRACE(
                "Clock stats: offset from master=[{}], ratio={}", filtered_offset_stats_.to_string(1000.0),
                local_clock_.get_frequency_ratio()
            );
        }
    }

    /**
     * Steps the clock to the given offset from the master clock.
     * @param offset_from_master_seconds The offset from the master clock in seconds.
     */
    void step_clock(const double offset_from_master_seconds) {
        TRACY_ZONE_SCOPED;

        local_clock_.step(offset_from_master_seconds);
        offset_stats_.reset();
        adjustments_since_last_step_ = 0;

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
    constexpr static size_t k_lock_threshold = 10;
    constexpr static double k_calibrated_threshold = 0.0018;
    constexpr static int64_t k_clock_step_threshold_seconds = 1;

    LocalSystemClock& local_clock_;

    SlidingStats offset_stats_ {51};
    SlidingStats filtered_offset_stats_ {51};
    size_t adjustments_since_last_step_ {};
    Throttle<void> trace_adjustments_throttle_ {std::chrono::seconds(5)};
};

}  // namespace rav::ptp
