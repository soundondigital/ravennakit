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
#include <type_traits>
#include <cstdint>
#include <optional>

namespace rav {

/**
 * Represents sequence numbers with rollover (wraparound) semantics. This class is designed to work with any unsigned
 * integer type that wraps around to 0 after reaching its maximum value. It also supports handling gaps, making it
 * suitable for scenarios involving packet loss.
 * Use this class for the sequence id in PTP, sequence number in RTP, timestamp in RTP, etc.
 */
template<class T>
class wrapping_uint {
    static_assert(std::is_integral_v<T>, "sequence_number only supports integral types");
    static_assert(std::is_unsigned_v<T>, "sequence_number only supports unsigned types");

  public:
    /**
     * Default construct a sequence number with the value 0.
     */
    wrapping_uint() = default;

    /**
     * Construct a sequence number with the given value.
     * @param value The initial value of the sequence number.
     */
    explicit wrapping_uint(const T value) : value_(value) {}

    /**
     * Updates the value in the sequence. The number of steps taken from the previous value to the next value
     * will be returned, taking into account wraparound. The current (internal) value will only progress forward, if the
     * value is older than the current value, it will return std::nullopt. The returned value can be used to detect
     * gaps (when value > 1).
     *
     * @param value The value to set.
     * @return The difference between the given value and the current value, taking into account wraparound. If the
     * value is older than the current value, std::nullopt will be returned.
     */
    std::optional<T> update(const T value) {
        if (is_older_than(value, value_)) {
            return std::nullopt;  // Value too old
        }

        const auto diff = value - value_;
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
    wrapping_uint& operator=(const T value) {
        value_ = value;
        return *this;
    }

    /**
     * Increments the sequence number by 1 using modulo arithmetic.
     * @param value The value to increment by.
     * @return This instance.
     */
    wrapping_uint& operator+=(const T value) {
        value_ += value;
        return *this;
    }

    /**
     * Decrements the sequence number by 1 using modulo arithmetic.
     * @param value The value to decrement by.
     * @return This instance.
     */
    wrapping_uint& operator-=(const T value) {
        value_ -= value;
        return *this;
    }

    /**
     * Increments the sequence number by the given value.
     * @param value The value to increment by.
     * @return A new sequence number instance.
     */
    wrapping_uint operator+(const T value) const {
        return wrapping_uint(value_ + value);
    }

    /**
     * Decrements the sequence number by the given value.
     * @param value The value to decrement by.
     * @return A new sequence number instance.
     */
    wrapping_uint operator-(const T value) const {
        return wrapping_uint(value_ - value);
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
    friend bool operator==(const wrapping_uint& lhs, const wrapping_uint& rhs) {
        return lhs.value_ == rhs.value_;
    }

    /**
     * Compares the sequence number with the given value.
     * @param lhs Left hand side.
     * @param rhs Right hand side.
     * @return True if the sequence number is not equal to the given value, false otherwise.
     */
    friend bool operator!=(const wrapping_uint& lhs, const wrapping_uint& rhs) {
        return lhs.value_ != rhs.value_;
    }

    /**
     * Compares the sequence number with the given value.
     * @param lhs Left hand side.
     * @param rhs Right hand side.
     * @return True if the sequence number is less than the given value, false otherwise.
     */
    friend bool operator<(const wrapping_uint& lhs, const wrapping_uint& rhs) {
        return is_older_than(lhs.value_, rhs.value_);
    }

    /**
     * Compares the sequence number with the given value.
     * @param lhs Left hand side.
     * @param rhs Right hand side.
     * @return True if the sequence number is less than or equal to the given value, false otherwise.
     */
    friend bool operator<=(const wrapping_uint& lhs, const wrapping_uint& rhs) {
        return lhs < rhs || lhs == rhs;
    }

    /**
     * Compares the sequence number with the given value.
     * @param lhs Left hand side.
     * @param rhs Right hand side.
     * @return True if the sequence number is greater than the given value, false otherwise.
     */
    friend bool operator>(const wrapping_uint& lhs, const wrapping_uint& rhs) {
        return !(lhs <= rhs);
    }

    /**
     * Compares the sequence number with the given value.
     * @param lhs Left hand side.
     * @param rhs Right hand side.
     * @return True if the sequence number is greater than or equal to the given value, false otherwise.
     */
    friend bool operator>=(const wrapping_uint& lhs, const wrapping_uint& rhs) {
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

/// 8-bit wrapping unsigned integer.
using wrapping_uint8 = wrapping_uint<uint8_t>;

/// 16-bit wrapping unsigned integer.
using wrapping_uint16 = wrapping_uint<uint16_t>;

/// 32-bit wrapping unsigned integer.
using wrapping_uint32 = wrapping_uint<uint32_t>;

/// 64-bit wrapping unsigned integer.
using wrapping_uint64 = wrapping_uint<uint64_t>;

}  // namespace rav
