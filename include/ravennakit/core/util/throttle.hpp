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

#include <chrono>

namespace rav {

/**
 * A class that throttles a value to a given interval.
 * @tparam T The type of the value to throttle.
 */
template<class T>
class throttle {
  public:
    throttle() = default;

    /**
     * Constructs the throttle with the given interval.
     * @param interval The interval to throttle the value to.
     */
    explicit throttle(const std::chrono::milliseconds interval) : interval_(interval) {}

    /**
     * @param interval The interval to throttle the value to.
     */
    void set_interval(const std::chrono::milliseconds interval) {
        interval_ = interval;
    }

    /**
     * Updates the value and if the interval has passed since the last update returns the new value. If the value is
     * equal to the current value, nothing will happen and the function will return an empty optional.
     * @param value The new value.
     * @return The value if changed and the interval was passed, otherwise an empty optional.
     */
    std::optional<T> update(T value) {
        value_ = value;
        return get_throttled();
    }

    /**
     * @return The value, which might be empty if no value was set before.
     */
    std::optional<T> get() {
        return value_;
    }

    /**
     * @return The value if the interval has passed since the last update, otherwise an empty optional. The last set
     * value will be returned, even if the value wasn't changed since the last call to update.
     */
    std::optional<T> get_throttled() {
        if (!value_.has_value()) {
            return {};
        }
        if (const auto now = std::chrono::steady_clock::now(); now > last_update_ + interval_) {
            last_update_ = now;
            return value_;
        }
        return {};
    }

    /**
     * Clears the stored value.
     */
    void clear() {
        value_.reset();
    }

  private:
    std::optional<T> value_ {};
    std::chrono::steady_clock::time_point last_update_ {};
    std::chrono::milliseconds interval_ {100};
};

/**
 * Specialization for void, which doesn't store a value.
 */
template<>
class throttle<void> {
public:
    throttle() = default;

    /**
     * Constructs the throttle with the given interval.
     * @param interval The interval to throttle the value to.
     */
    explicit throttle(const std::chrono::milliseconds interval) : interval_(interval) {}

    /**
     * @param interval The interval to throttle the value to.
     */
    void set_interval(const std::chrono::milliseconds interval) {
        interval_ = interval;
    }

    /**
     * Updates the value and if the interval has passed since the last update returns the new value. If the value is
     * equal to the current value, nothing will happen and the function will return an empty optional.
     * @return The value if changed and the interval was passed, otherwise an empty optional.
     */
    bool update() {
        const auto now = std::chrono::steady_clock::now();
        if (now > last_update_ + interval_) {
            last_update_ = now;
            return true;
        }
        return false;
    }

private:
    std::chrono::steady_clock::time_point last_update_ {};
    std::chrono::milliseconds interval_ {100};
};

}  // namespace rav
