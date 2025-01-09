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

#include "ravennakit/core/types/safe_int.hpp"

#include <cstdint>
#include <cmath>

namespace rav {

/**
 * Represents a point in time. It stores this point as a combination of seconds + nanoseconds + fraction.
 */
class ptp_time_interval {
  public:
    ptp_time_interval() = default;

    /**
     * Constructs a ptp_time_interval from nanoseconds and fraction.
     * Ensures that the fraction part is normalized to always be positive.
     * @param seconds The number of seconds.
     * @param nanos The number of nanoseconds.
     * @param fraction The nanosecond fraction part.
     */
    ptp_time_interval(const int64_t seconds, const int32_t nanos, const uint16_t fraction) :
        seconds_(seconds), nanos_(nanos) {
        seconds_ = seconds;
        nanos_ = nanos * k_fractional_scale + fraction;
        normalize();
    }

    /**
     * @return The number of seconds.
     */
    [[nodiscard]] int64_t seconds() const {
        return seconds_;
    }

    /**
     * @return The number of nanoseconds, including the seconds part. If the value is too big to represent as a 64-bit
     * integer, the result is undefined.
     */
    [[nodiscard]] int64_t nanos() const {
        return seconds_ * 1'000'000'000 + nanos_ / k_fractional_scale;
    }

    /**
     * @return The number of nanoseconds, rounded to the nearest nanosecond.
     */
    [[nodiscard]] int64_t nanos_rounded() const {
        if (fraction_raw() >= k_fractional_scale / 2) {
            return nanos() + 1;
        }
        return nanos();
    }

    /**
     * @return The number of nanoseconds, without taking seconds into account and without fraction.
     */
    [[nodiscard]] int64_t nanos_raw() const {
        return nanos_ / k_fractional_scale;
    }

    /**
     * @return The number of fractional nanoseconds, without taking seconds into account.
     */
    [[nodiscard]] uint16_t fraction_raw() const {
        return static_cast<uint16_t>(nanos_ % k_fractional_scale);
    }

    /**
     * Create a ptp_time_interval from a wire format value where the nanoseconds are in the high 48 bits and the
     * fraction is in the low 16 bits.
     * @param value The wire format value.
     * @return The ptp_time_interval.
     */
    static ptp_time_interval from_wire_format(const int64_t value) {
        auto nanoseconds = value >> 16;
        const auto seconds = nanoseconds / 1'000'000'000;
        nanoseconds -= seconds * 1'000'000'000;
        return {seconds, static_cast<int32_t>(nanoseconds), static_cast<uint16_t>((value & 0xffff))};
    }

    /**
     * Convert the ptp_time_interval to wire format where the nanoseconds are in the high 48 bits and the fraction is in
     * the low 16 bits.
     * @return The wire format value.
     */
    [[nodiscard]] int64_t to_wire_format() const {
        const auto r = safe_int64(seconds_) * 1'000'000'000 * k_fractional_scale + nanos_;

        if (r.expected()) {
            return r.value();
        }

        switch (r.error()) {
            case safe_int_error::overflow:
                return std::numeric_limits<int64_t>::max();
            case safe_int_error::underflow:
                return std::numeric_limits<int64_t>::min();
            case safe_int_error::div_by_zero:
            default:
                RAV_ASSERT_FALSE("Division by zero when converting to wire format");
                return {};
        }
    }

    /**
     * Adds two ptp_time_intervals together.
     * @param other The time interval to add.
     * @return The sum of the two time intervals.
     */
    ptp_time_interval operator+(const ptp_time_interval& other) const {
        ptp_time_interval r;
        r.seconds_ = seconds_ + other.seconds_;
        r.nanos_ = nanos_ + other.nanos_;
        r.normalize();
        return r;
    }

    /**
     * Subtracts two ptp_time_intervals.
     * @param other The time interval to subtract.
     * @return The difference of the two time intervals.
     */
    ptp_time_interval operator-(const ptp_time_interval& other) const {
        ptp_time_interval r;
        r.seconds_ = seconds_ - other.seconds_;
        r.nanos_ = nanos_ - other.nanos_;
        r.normalize();
        return r;
    }

    /**
     * Divide the time interval by a scalar.
     * @param other The scalar to divide by.
     * @return A reference to this object.
     */
    ptp_time_interval operator/(const int64_t& other) const {
        ptp_time_interval r = *this;
        r /= other;
        return r;
    }

    /**
     * Multiply the time interval by a scalar.
     * @param other The scalar to multiply by.
     * @return A reference to this object.
     */
    ptp_time_interval operator*(const int64_t& other) const {
        ptp_time_interval r = *this;
        r *= other;
        return r;
    }

    /**
     * Adds another ptp_time_interval to this one.
     * @param other The time interval to add.
     * @return A reference to this object.
     */
    ptp_time_interval& operator+=(const ptp_time_interval& other) {
        seconds_ += other.seconds_;
        nanos_ += other.nanos_;
        normalize();
        return *this;
    }

    /**
     * Subtracts another ptp_time_interval from this one.
     * @param other The time interval to subtract.
     * @return A reference to this object.
     */
    ptp_time_interval& operator-=(const ptp_time_interval& other) {
        seconds_ -= other.seconds_;
        nanos_ -= other.nanos_;
        normalize();
        return *this;
    }

    /**
     * Divide the time interval by a scalar.
     * @param other The scalar to divide by.
     * @return A reference to this object.
     */
    ptp_time_interval& operator/=(const int64_t& other) {
        nanos_ += seconds_ % other * 1'000'000'000 * k_fractional_scale;
        seconds_ /= other;
        nanos_ /= other;
        normalize();
        return *this;
    }

    /**
     * Multiply the time interval by a scalar.
     * @param other The scalar to multiply by.
     * @return A reference to this object.
     */
    ptp_time_interval& operator*=(const int64_t& other) {
        seconds_ *= other;
        const auto fraction = static_cast<uint16_t>(nanos_ & 0xffff) * std::abs(other);
        nanos_ = (nanos_ >> 16 << 16) * other;
        nanos_ += fraction;
        normalize();
        return *this;
    }

    /**
     * Equality operator.
     * @param other The time interval to compare to.
     * @return True if the time intervals are equal, false otherwise.
     */
    bool operator==(const ptp_time_interval& other) const {
        return seconds_ == other.seconds_ && nanos_ == other.nanos_;
    }

    /**
     * Inequality operator.
     * @param other The time interval to compare to.
     * @return True if the time intervals are not equal, false otherwise.
     */
    bool operator!=(const ptp_time_interval& other) const {
        return !(*this == other);
    }

    constexpr static int64_t k_fractional_scale = 0x10000;

  private:
    int64_t seconds_ {};  // 48 bits on the wire
    int64_t nanos_ {};    // [0, 1'000'000'000) including 16-bit fraction

    /**
     * Normalizes the time interval such that:
     * - nanos_ is always in the range [0, 1'000'000'000 * k_fractional_scale).
     * - Adjusts seconds_ accordingly.
     */
    void normalize() {
        if (nanos_ >= 1'000'000'000 * k_fractional_scale) {
            const auto carry = nanos_ / 1'000'000'000 / k_fractional_scale;
            seconds_ += carry;
            nanos_ -= carry * k_fractional_scale * 1'000'000'000;
        } else if (nanos_ < 0) {
            auto borrow = std::abs(nanos_ / 1'000'000'000 / k_fractional_scale);
            const auto remainder = nanos_ % (1'000'000'000 * k_fractional_scale);
            if (remainder != 0) {
                borrow += 1;
            }
            seconds_ -= borrow;
            nanos_ += borrow * k_fractional_scale * 1'000'000'000;
        }
    }
};

}  // namespace rav
