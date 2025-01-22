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

#include <limits>

namespace rav {

/**
 * Represents sequence numbers with rollover (wraparound) semantics. This class is designed to work with any unsigned
 * integer type that wraps around to 0 after reaching its maximum value. It also supports handling gaps, making it
 * suitable for scenarios involving packet loss.
 * Use this class for the sequence id in PTP, sequence number in RTP, timestamp in RTP, etc.
 * TODO: Rename to wrapping_uint
 */
template<class T>
class sequence_number {
    static_assert(std::is_integral_v<T>, "sequence_number only supports integral types");
    static_assert(std::is_unsigned_v<T>, "sequence_number only supports unsigned types");

  public:
    /**
     * Default construct a sequence number with the value 0.
     */
    sequence_number() = default;

    /**
     * Construct a sequence number with the given value.
     * @param value The initial value of the sequence number.
     */
    explicit sequence_number(const T value) : value_(value) {}

    /**
     * Sets a next value in the sequence. The number of steps taken from the previous value to the next value
     * will be returned, taking into account wraparound. If the value is too old, it will be discarded. This number can
     * be used to detect gaps.
     *
     * @param value The value to set.
     * @return The difference between the given value and the current value, taking into account wraparound. If the
     * value is equal to the current value, or too old, 0 will be returned.
     */
    T set_next(const T value) {
        if (is_older_than(value, value_)) {
            return 0;  // Value too old
        }

        auto diff = value - value_;
        value_ = value;
        return static_cast<T>(diff);
    }

    /**
     * @returns The value of the sequence number.
     */
    explicit operator T() const {
        return value_;
    }

    /**
     * @returns The value of the sequence number.
     */
    T value() const {
        return value_;
    }

    /**
     * Assigns a new value to the sequence number.
     * @param value The new value of the sequence number.
     * @return This instance.
     */
    sequence_number& operator=(const T value) {
        value_ = value;
        return *this;
    }

    /**
     * Increments the sequence number by 1 using modulo arithmetic.
     * @param value The value to increment by.
     * @return This instance.
     */
    sequence_number& operator+=(const T value) {
        value_ += value;
        return *this;
    }

    /**
     * Decrements the sequence number by 1 using modulo arithmetic.
     * @param value The value to decrement by.
     * @return This instance.
     */
    sequence_number& operator-=(const T value) {
        value_ -= value;
        return *this;
    }

    /**
     * Increments the sequence number by the given value.
     * @param value The value to increment by.
     * @return A new sequence number instance.
     */
    sequence_number operator+(const T value) {
        return sequence_number(value_ + value);
    }

    /**
     * Decrements the sequence number by the given value.
     * @param value The value to decrement by.
     * @return A new sequence number instance.
     */
    sequence_number operator-(const T value) {
        return sequence_number(value_ - value);
    }

    /**
     * Compares the sequence number with the given value.
     * @param other The value to compare with.
     * @return True if the sequence number is equal to the given value, false otherwise.
     */
    bool operator==(T other) {
        return value_ == other;
    }

    /**
     * Compares the sequence number with the given value.
     * @param other The value to compare with.
     * @return True if the sequence number is not equal to the given value, false otherwise.
     */
    bool operator!=(T other) {
        return value_ != other;
    }

    /**
     * Compares the sequence number with the given value.
     * @param lhs Left hand side.
     * @param rhs Right hand side.
     * @return True if the sequence number is less than the given value, false otherwise.
     */
    friend bool operator==(const sequence_number& lhs, const sequence_number& rhs) {
        return lhs.value_ == rhs.value_;
    }

    /**
     * Compares the sequence number with the given value.
     * @param lhs Left hand side.
     * @param rhs Right hand side.
     * @return True if the sequence number is not equal to the given value, false otherwise.
     */
    friend bool operator!=(const sequence_number& lhs, const sequence_number& rhs) {
        return lhs.value_ != rhs.value_;
    }

    /**
     * Compares the sequence number with the given value.
     * @param lhs Left hand side.
     * @param rhs Right hand side.
     * @return True if the sequence number is less than the given value, false otherwise.
     */
    friend bool operator<(const sequence_number& lhs, const sequence_number& rhs) {
        return is_older_than(lhs.value_, rhs.value_);
    }

    /**
     * Compares the sequence number with the given value.
     * @param lhs Left hand side.
     * @param rhs Right hand side.
     * @return True if the sequence number is less than or equal to the given value, false otherwise.
     */
    friend bool operator<=(const sequence_number& lhs, const sequence_number& rhs) {
        return lhs < rhs || lhs == rhs;
    }

    /**
     * Compares the sequence number with the given value.
     * @param lhs Left hand side.
     * @param rhs Right hand side.
     * @return True if the sequence number is greater than the given value, false otherwise.
     */
    friend bool operator>(const sequence_number& lhs, const sequence_number& rhs) {
        return !(lhs <= rhs);
    }

    /**
     * Compares the sequence number with the given value.
     * @param lhs Left hand side.
     * @param rhs Right hand side.
     * @return True if the sequence number is greater than or equal to the given value, false otherwise.
     */
    friend bool operator>=(const sequence_number& lhs, const sequence_number& rhs) {
        return lhs > rhs || lhs == rhs;
    }

  private:
    T value_ {};

    /**
     * Checks if a is older than b, taking into account wraparound.
     * @param a The first value.
     * @param b The second value.
     * @return True if a is older than b, false otherwise.
     */
    static bool is_older_than(T a, T b) {
        return !(a == b) && static_cast<T>(b - a) < std::numeric_limits<T>::max() / 2 + 1;
    }
};

}  // namespace rav
