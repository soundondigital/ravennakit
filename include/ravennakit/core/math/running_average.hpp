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

#include <type_traits>

namespace rav {

/**
 * A running average.
 */
class running_average {
public:
    /**
     * Adds a new value to the running average.
     * @param value The value to add.
     */
    void add(const double value) {
        count_++;
        average_ += (value - average_) / static_cast<double>(count_);
    }

    /**
     * Adds a new value to the running average.
     * @tparam U The type of the value to add.
     * @param value The value to add.
     */
    template<class U>
    void add(U value) {
        add(static_cast<double>(value));
    }

    /**
     * @return The current average.
     */
    [[nodiscard]] double average() const {
        return average_;
    }

    /**
     * @returns The number of values added to the running average.
     */
    [[nodiscard]] size_t count() const {
        return count_;
    }

    /**
     * Resets the running average.
     */
    void reset() {
        count_ = 0;
        average_ = 0;
    }

private:
    double average_ {};
    size_t count_ {};
};

}  // namespace rav
