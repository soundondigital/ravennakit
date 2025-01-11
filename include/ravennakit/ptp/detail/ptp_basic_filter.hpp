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
    double update(const double value) {
        auto clamped_offset = value;
        if (std::fabs(value) > offset_confidence_) {
            clamped_offset = std::clamp(value, -offset_confidence_, offset_confidence_);
            offset_confidence_ *= 2;
        } else {
            offset_confidence_ -= (offset_confidence_ - std::fabs(value)) * gain_;
        }

        return clamped_offset * gain_;
    }

    /**
     * Resets the filter.
     */
    void reset() {
        offset_confidence_ = 1.0;
    }

  private:
    double offset_confidence_ {1.0};  // In seconds
    double gain_ {1.0};
};

}  // namespace rav
