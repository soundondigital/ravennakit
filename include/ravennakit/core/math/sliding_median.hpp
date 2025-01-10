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

#include "ravennakit/core/containers/ring_buffer.hpp"

#include <vector>

namespace rav {

class sliding_median {
  public:
    explicit sliding_median(const size_t size) : window_(size), buffer_(size) {}

    /**
     * Adds a new value to the sliding window average.
     * @param value The value to add.
     */
    void add(const double value) {
        window_.push_back(value);
    }

    /**
     * @return The median of the values in the window.
     */
    double median() {
        buffer_.clear();

        for (auto& value : window_) {
            buffer_.push_back(value);
        }

        if (buffer_.empty()) {
            return 0.0;
        }

        std::sort(buffer_.begin(), buffer_.end());

        const size_t n = buffer_.size();
        if (n % 2 == 1) {
            // Odd: Return the middle element
            return buffer_[n / 2];
        }

        // Even: Return the average of the two middle elements
        return (buffer_[n / 2 - 1] + buffer_[n / 2]) / 2.0;
    }

    /**
     * @return The number of values added to the sliding window average.
     */
    [[nodiscard]] size_t count() const {
        return window_.size();
    }

    /**
     * Resets the sliding window average.
     */
    void reset() {
        window_.clear();
        buffer_.clear();
    }

  private:
    ring_buffer<double> window_;
    std::vector<double> buffer_;
};

}  // namespace rav
