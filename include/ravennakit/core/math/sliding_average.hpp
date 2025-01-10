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

namespace rav {

/**
 * A class that calculates the average of the last N values added to it.
 */
class sliding_average {
  public:
    explicit sliding_average(const size_t size) : window_(size) {}

    /**
     * Adds a new value to the sliding window average.
     * @param value The value to add.
     */
    void add(const double value) {
        if (window_.full()) {
            sum_ -= window_.pop_front().value();
        }

        window_.push_back(value);
        sum_ += value;
    }

    /**
     * @return The average of the values in the window.
     */
    [[nodiscard]] double average() const {
        if (window_.empty()) {
            return 0.0;
        }
        return sum_ / static_cast<double>(window_.size());
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
        sum_ = 0;
    }

  private:
    ring_buffer<double> window_;
    double sum_{};
};

}  // namespace rav
