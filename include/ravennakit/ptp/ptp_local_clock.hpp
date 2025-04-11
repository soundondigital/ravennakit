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

#include "ravennakit/core/tracy.hpp"
#include "ravennakit/core/chrono/high_resolution_clock.hpp"
#include "ravennakit/core/sync/triple_buffer.hpp"
#include "types/ptp_timestamp.hpp"

namespace rav::ptp {

/**
 * Maintains a local clock corrected to the timebase of another time source, most likely a PTP master clock.
 */
class LocalClock {
  public:
    /**
     * @return The best estimate of 'now' in the timescale of the grand master clock.
     */
    [[nodiscard]] Timestamp now() const {
        return get_adjusted_time(system_monotonic_now());
    }

    /**
     * Returns the adjusted time of the clock, which is the time in the timescale of the grand master clock.
     * @param system_time The system time to adjust.
     * @return The adjusted time in the timescale of the grand master clock.
     */
    [[nodiscard]] Timestamp get_adjusted_time(const Timestamp system_time) const {
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
        adjustments_since_last_step_++;
    }

    /**
     * Steps the clock to the given offset from the master clock. This is used when the clock is out of sync and needs
     * to be reset.
     * @param offset_from_master The offset from the master clock in seconds.
     */
    void step(const double offset_from_master) {
        TRACY_ZONE_SCOPED;
        last_sync_ = system_monotonic_now();
        shift_ += -offset_from_master;
        frequency_ratio_ = 1.0;
        adjustments_since_last_step_ = 0;
    }

    /**
     * @return The current frequency ratio of the clock.
     */
    [[nodiscard]] double get_frequency_ratio() const {
        return frequency_ratio_;
    }

    /**
     *
     * @return The current shift of the clock.
     */
    [[nodiscard]] double get_shift() const {
        return shift_;
    }

    /**
     * @return True if the clock is valid, false otherwise. It does this by checking if the last sync time is valid.
     */
    [[nodiscard]] bool is_valid() const {
        return last_sync_.valid();
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
     * Sets the calibrated state of the clock. A clock is considered calibrated when it has received enough adjustments
     * and is within the calibrated threshold.
     */
    void set_calibrated(const bool calibrated) {
        calibrated_ = calibrated;
    }

    /**
     * @return True if the clock is calibrated, false otherwise. A clock is considered calibrated when it has received
     * enough adjustments and is within the calibrated threshold.
     */
    [[nodiscard]] bool is_calibrated() const {
        return is_locked() && calibrated_;
    }

  private:
    constexpr static size_t k_lock_threshold = 10;

    Timestamp last_sync_ {};
    double shift_ {};
    double frequency_ratio_ = 1.0;
    size_t adjustments_since_last_step_ {};
    bool calibrated_ = false;

    static Timestamp system_monotonic_now() {
        return Timestamp(HighResolutionClock::now());
    }
};

}  // namespace rav::ptp
