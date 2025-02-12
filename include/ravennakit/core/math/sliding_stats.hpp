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
#include "ravennakit/core/util.hpp"

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
    explicit sliding_stats(const size_t size) : window_(size), sorted_data_(size) {}

    /**
     * Adds a new value and recalculates the statistics.
     * @param value The value to add.
     */
    void add(const double value) {
        window_.push_back(value);
        recalculate();
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
     * @return The variance of the values in the window.
     */
    [[nodiscard]] double variance() const {
        if (window_.empty()) {
            return 0.0;
        }
        double variance = 0.0;
        for (auto& v : window_) {
            variance += (v - average_) * (v - average_);
        }
        return variance / static_cast<double>(window_.size());
    }

    /**
     * @return The standard deviation of the values in the window.
     */
    [[nodiscard]] double standard_deviation() const {
        return standard_deviation(variance());
    }

    /**
     * @return The minimum value in the window.
     */
    [[nodiscard]] double min() const {
        return min_;
    }

    /**
     * @return The maximum value in the window.
     */
    [[nodiscard]] double max() const {
        return max_;
    }

    /**
     * @return The number of values added to the sliding window average.
     */
    [[nodiscard]] size_t count() const {
        return window_.size();
    }

    /**
     * @return True if the window is full, or false otherwise.
     */
    [[nodiscard]] bool full() const {
        return window_.full();
    }

    /**
     * Checks if the current value is an outlier compared to the median.
     * @param value The value to check.
     * @param threshold The threshold for the outlier check.
     * @return True if the current value is an outlier.
     */
    [[nodiscard]] bool is_outlier_median(const double value, const double threshold) const {
        return std::fabs(value - median_) > threshold;
    }

    /**
     * Checks if the current value is an outlier compared to the average.
     * @param value The value to check.
     * @param threshold The threshold for the outlier check.
     * @return True if the current value is an outlier.
     */
    [[nodiscard]] bool is_outlier_zscore(const double value, const double threshold) const {
        const auto stddev = standard_deviation();
        if (util::is_within(stddev, 0.0, 0.0)) {
            return false;
        }
        return std::fabs((value - average_) / stddev) > threshold;
    }

    /**
     * @param multiply_factor The factor to multiply the values with.
     * @return The values in the window as a string.
     */
    [[nodiscard]] std::string to_string(const double multiply_factor = 1.0) const {
        const auto v = variance();
        return fmt::format(
            "average={}, median={}, min={}, max={}, variance={}, stddev={}, count={}", average_ * multiply_factor,
            median_ * multiply_factor, min_ * multiply_factor, max_ * multiply_factor, v * multiply_factor,
            standard_deviation(v) * multiply_factor, window_.size()
        );
    }

    /**
     * Resets the sliding window average.
     */
    void reset() {
        window_.clear();
        sorted_data_.clear();
        median_ = {};
        average_ = {};
        min_ = {};
        max_ = {};
    }

  private:
    ring_buffer<double> window_;
    std::vector<double> sorted_data_;
    double average_ {};  // Last calculated average value
    double median_ {};   // Last calculated median value
    double min_ {};      // Last calculated minimum value
    double max_ {};      // Last calculated maximum value

    /**
     * @return The standard deviation of the values in the window.
     */
    [[nodiscard]] static double standard_deviation(const double variance) {
        return std::sqrt(variance);
    }

    void recalculate() {
        if (window_.empty()) {
            median_ = 0.0;
            average_ = 0.0;
            return;
        }
        min_ = window_.front();
        max_ = window_.front();
        double sum = 0.0;
        sorted_data_.clear();
        for (auto& e : window_) {
            sum += e;
            sorted_data_.push_back(e);
            min_ = std::min(min_, e);
            max_ = std::max(max_, e);
        }
        average_ = sum / static_cast<double>(window_.size());
        std::sort(sorted_data_.begin(), sorted_data_.end());
        const size_t n = sorted_data_.size();
        if (n % 2 == 1) {
            median_ = sorted_data_[n / 2];  // Odd: return the middle element
            return;
        }
        // Even: return the average of the two middle elements
        median_ = (sorted_data_[n / 2 - 1] + sorted_data_[n / 2]) / 2.0;
    }
};

}  // namespace rav
