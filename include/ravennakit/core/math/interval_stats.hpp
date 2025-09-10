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

#include "ravennakit/ptp/detail/ptp_basic_filter.hpp"

#include <cstdint>

namespace rav {

/**
 * Keeps track of an ema with outliers filtered out and a max deviation.
 */
struct IntervalStats {
    /// Interval as measured over time
    double interval = 0.0;
    /// The max deviation compared to interval
    double max_deviation = 0.0;
    /// Whether this struct has been initialized
    bool initialized = false;
    /// The alpa of ema calculation. Lower is smoother.
    double alpha = 0.001;

    void update(const double interval_ms) {
        if (!initialized) {
            interval = interval_ms;
            initialized = true;
            return;
        }

        const auto ema = alpha * interval_ms + (1.0 - alpha) * interval;
        const auto step = ema - interval;

        if (step > current_step_size_) {
            interval += current_step_size_;  // Limit positive change
            current_step_size_ = std::min(current_step_size_ * 2.0, k_max_step_size);
        } else if (step < -current_step_size_) {
            interval -= current_step_size_;  // Limit negative change
            current_step_size_ = std::min(current_step_size_ * 2.0, k_max_step_size);
        } else {
            interval = ema;  // Change is within limit
            current_step_size_ = std::max(current_step_size_ / 2.0, k_min_step_size);
        }

        max_deviation = std::max(std::fabs(interval_ms - interval), max_deviation);
    }

private:
    static constexpr auto k_min_step_size = 0.00001;
    static constexpr auto k_max_step_size = 100'000.0;
    double current_step_size_ = k_min_step_size;
};

}  // namespace rav
