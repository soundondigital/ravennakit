/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include "assert.hpp"

namespace rav {

/**
 * Represents a range of values. Both start and end are inclusive. Range must be valid (start <= end).
 * @tparam T The value type.
 */
template<class T>
class range {
  public:
    range() = default;

    constexpr range(T start, T end) : start_(start), end_(end) {
        RAV_ASSERT(start_ <= end_, "Invalid range");
    }

    /**
     * @return The start of the range.
     */
    [[nodiscard]] T start() const {
        return start_;
    }

    /**
     * @return The end of the range.
     */
    [[nodiscard]] T end() const {
        return end_;
    }

    /**
     * Check if the value is within the range. Both start and end are inclusive.
     * @param value The value to check.
     * @return True if the value is within the range, false otherwise.
     */
    [[nodiscard]] bool contains(const T& value) const {
        return value >= start_ && value <= end_;
    }

  private:
    T start_ {};
    T end_ {};
};

}  // namespace rav
