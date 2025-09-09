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
    double ema = 0.0;
    double max_deviation = 0.0;
    bool initialized = false;
    double alpha = 0.001;
    double rejection_factor = 1.5;

    void update(const double interval_ms) {
        if (!initialized) {
            ema = interval_ms;
            initialized = true;
            return;
        }

        // Check if this is an extreme outlier
        const double ratio = interval_ms / ema;
        if (ratio > rejection_factor || ratio < (1.0 / rejection_factor)) {
            const double deviation = std::fabs(interval_ms - ema);
            if (deviation > max_deviation) {
                max_deviation = deviation;
            }
            return;
        }

        ema = alpha * interval_ms + (1.0 - alpha) * ema;
        max_deviation = std::max(std::fabs(interval_ms - ema), max_deviation);
    }

    [[nodiscard]] bool is_outlier(const double interval_ms) const {
        if (!initialized) {
            return false;
        }
        const double ratio = interval_ms / ema;
        return (ratio > rejection_factor || ratio < (1.0 / rejection_factor));
    }
};

}  // namespace rav
