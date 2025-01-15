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

#include <cmath>
#include <algorithm>

namespace rav {

/**
 * Simple averaging filter.
 */
class ptp_basic_filter {
  public:
    /**
     * Constructor.
     * @param gain The gain of the filter.
     */
    explicit ptp_basic_filter(const double gain) : gain_(gain) {
        reset();
    }

    /**
     * Updates the filter with a new value.
     * @param value The new value.
     * @return The filtered value.
     */
    double update(double value) {
        const auto value_abs = std::fabs(value);
        if (value_abs > confidence_range_) {
            confidence_range_ *= 2.0;
            value = std::clamp(value, -confidence_range_, confidence_range_);
        } else {
            confidence_range_ -= (confidence_range_ - value_abs) * gain_;
        }
        return value * gain_;
    }

    /**
     * Resets the filter.
     */
    void reset() {
        confidence_range_ = 1.0;
    }

  private:
    double confidence_range_ {1.0};  // In seconds
    double gain_ {1.0};
};

}  // namespace rav
