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
#include <algorithm>

namespace rav {

/**
 * Values can be added after which different calculations can be performed on the values. The values are stored in a
 * ring buffer to keep track of the last N values. Older values will be overwritten by newer values.
 */
class sliding_stats {
  public:
    /**
     * Constructor.
     * @param size The amount of elements to hold.
     */
    explicit sliding_stats(const size_t size) : window_(size), median_buffer_(size) {}

    /**
     * Adds a new value and recalculates the statistics.
     * @param value The value to add.
     */
    void add(const double value) {
        window_.push_back(value);
        recalculate_average();
        recalculate_median();
    }

    /**
     * @return The last calculated average of the values in the window.
     */
    [[nodiscard]] double average() const {
        return average_;
    }

    /**
     * @return The last calculated median of the values in the window.
     */
    [[nodiscard]] double median() const {
        return median_;
    }

    /**
     * @return The number of values added to the sliding window average.
     */
    [[nodiscard]] size_t count() const {
        return window_.size();
    }

    /**
     * Checks if the current value is an outlier compared to the median.
     * @param value The value to check.
     * @param threshold The threshold for the outlier check.
     * @return True if the current value is an outlier.
     */
    [[nodiscard]] bool is_outlier(const double value, const double threshold) const {
        return std::abs(value - median_) > threshold;
    }

    /**
     * Resets the sliding window average.
     */
    void reset() {
        window_.clear();
        median_buffer_.clear();
        median_ = {};
        average_ = {};
    }

  private:
    ring_buffer<double> window_;
    std::vector<double> median_buffer_;  // Used for median calculations
    double average_ {};                  // Last average value
    double median_ {};                   // Last median value

    void recalculate_average() {
        double sum = 0.0;
        for (const auto& e : window_) {
            sum += e;
        }
        average_ = sum / static_cast<double>(window_.size());
    }

    void recalculate_median() {
        median_buffer_.clear();
        for (auto& e : window_) {
            median_buffer_.push_back(e);
        }
        if (median_buffer_.empty()) {
            median_ = 0.0;
            return;
        }
        std::sort(median_buffer_.begin(), median_buffer_.end());
        const size_t n = median_buffer_.size();
        if (n % 2 == 1) {
            median_ = median_buffer_[n / 2];  // Odd: return the middle element
            return;
        }
        // Even: return the average of the two middle elements
        median_ = (median_buffer_[n / 2 - 1] + median_buffer_[n / 2]) / 2.0;
    }
};

}  // namespace rav
