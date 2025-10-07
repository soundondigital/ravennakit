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

#include "ptp_time_interval.hpp"
#include "ravennakit/core/byte_order.hpp"
#include "ravennakit/core/containers/buffer_view.hpp"
#include "ravennakit/core/containers/byte_buffer.hpp"
#include "ravennakit/core/streams/output_stream.hpp"
#include "ravennakit/core/types/uint48.hpp"

#include <ctime>

namespace rav::ptp {

/**
 * A PTP timestamp, consisting of a seconds and nanoseconds part.
 * Note: not suitable for memcpy to and from the wire.
 */
struct Timestamp {
    /// Size on the wire in bytes.
    static constexpr size_t k_size = 10;

    Timestamp() = default;

    /**
     * Create a ptp_timestamp from a number of nanoseconds.
     * @param nanos The number of nanoseconds.
     */
    explicit Timestamp(const uint64_t nanos) {
        seconds_ = nanos / 1'000'000'000;
        nanoseconds_ = static_cast<uint32_t>(nanos % 1'000'000'000);
    }

    /**
     * Create a ptp_timestamp from seconds and milliseconds.
     * @param seconds The number of seconds.
     * @param nanoseconds The number of nanoseconds.
     */
    Timestamp(const uint64_t seconds, const uint32_t nanoseconds) : seconds_(seconds), nanoseconds_(nanoseconds) {}

    /**
     * Get a timestamp for given amount of years since epoch (whatever that is in the callers context).
     * Not counting leap years.
     * @param years The number of years.
     * @return The timestamp.
     */
    static Timestamp from_years(const uint64_t years) {
        return {years * 365 * 24 * 60 * 60, 0};
    }

    /**
     * Get a timestamp for given amount of days since epoch (whatever that is in the callers context).
     * @param seconds The number of days.
     * @return The timestamp.
     */
    static Timestamp from_seconds(const uint64_t seconds) {
        return {seconds, 0};
    }

    /**
     * Adds two ptp_timestamps together.
     * @param other The timestamp to add.
     * @return The sum of the two timestamps.
     */
    [[nodiscard]] TimeInterval operator+(const Timestamp& other) const {
        return this->to_time_interval() + other.to_time_interval();
    }

    /**
     * Subtracts two ptp_timestamps.
     * @param other The timestamp to subtract.
     * @return The difference of the two timestamps.
     */
    [[nodiscard]] TimeInterval operator-(const Timestamp& other) const {
        return this->to_time_interval() - other.to_time_interval();
    }

    /**
     * Adds given time interval to the timestamp.
     * @param time_interval The interval to add.
     */
    void add(const TimeInterval time_interval) {
        if (time_interval.seconds() < 0) {
            // Subtract the value
            const auto s_abs = static_cast<uint64_t>(std::abs(time_interval.seconds()));
            nanoseconds_ += static_cast<uint32_t>(time_interval.nanos());
            if (nanoseconds_ >= 1'000'000'000) {
                seconds_ += 1;
                nanoseconds_ -= 1'000'000'000;
            }
            if (s_abs > seconds_) {
                RAV_LOG_WARNING("ptp_timestamp underflow");
                // TODO: This case should be reported to the caller
                *this = {};  // Prevent underflow
                return;
            }
            seconds_ -= s_abs;
        } else {
            // Add the value
            const auto seconds_delta = static_cast<uint64_t>(time_interval.seconds());
            const auto nanoseconds_delta = static_cast<uint32_t>(time_interval.nanos());
            seconds_ += seconds_delta;
            nanoseconds_ += nanoseconds_delta;
            if (nanoseconds_ >= 1'000'000'000) {
                seconds_ += 1;
                nanoseconds_ -= 1'000'000'000;
            }
        }
    }

    /**
     * Adds given amount of seconds to the timestamp.
     * @param time_interval_seconds The amount of seconds to add.
     */
    void add_seconds(const double time_interval_seconds) {
        double seconds {};
        const auto fractional = std::modf(time_interval_seconds, &seconds);
        const auto nanos = std::round(fractional * 1'000'000'000);

        if (time_interval_seconds < 0) {
            // Subtract the value
            const auto s_abs = std::fabs(seconds);
            const auto n_abs = std::fabs(nanos);

            if (n_abs > nanoseconds_) {
                if (seconds_ < 1) {
                    RAV_LOG_WARNING("ptp_timestamp underflow");
                    // TODO: This case should be reported to the caller
                    *this = {};  // Prevent underflow
                    return;
                }
                seconds_ -= 1;
                nanoseconds_ += 1'000'000'000;
            }
            if (s_abs > static_cast<double>(seconds_)) {
                *this = {};  // Prevent underflow
                return;
            }
            seconds_ -= static_cast<uint64_t>(s_abs);
            nanoseconds_ -= static_cast<uint32_t>(n_abs);
        } else {
            // Add the value
            seconds_ += static_cast<uint64_t>(seconds);
            nanoseconds_ += static_cast<uint32_t>(nanos);
            if (nanoseconds_ >= 1'000'000'000) {
                seconds_ += 1;
                nanoseconds_ -= 1'000'000'000;
            }
        }
    }

    friend bool operator<(const Timestamp& lhs, const Timestamp& rhs) {
        return lhs.seconds_ < rhs.seconds_ || (lhs.seconds_ == rhs.seconds_ && lhs.nanoseconds_ < rhs.nanoseconds_);
    }

    friend bool operator<=(const Timestamp& lhs, const Timestamp& rhs) {
        return rhs >= lhs;
    }

    friend bool operator>(const Timestamp& lhs, const Timestamp& rhs) {
        return rhs < lhs;
    }

    friend bool operator>=(const Timestamp& lhs, const Timestamp& rhs) {
        return !(lhs < rhs);
    }

    /**
     * Create a ptp_timestamp from a buffer_view. Data is assumed to be valid, and the buffer_view must be at least 10
     * bytes long. No bounds checking is performed.
     * @param data The data to create the timestamp from. Assumed to be in network byte order.
     * @return The created ptp_timestamp.
     */
    static Timestamp from_data(const BufferView<const uint8_t> data) {
        RAV_ASSERT(data.size() >= 10, "data is too short to create a ptp_timestamp");
        Timestamp ts;
        ts.seconds_ = data.read_be<uint48_t>(0).to_uint64();
        ts.nanoseconds_ = data.read_be<uint32_t>(6);
        return ts;
    }

    /**
     * Write the ptp_announce_message to a byte buffer.
     * @param buffer The buffer to write to.
     */
    void write_to(ByteBuffer& buffer) const {
        buffer.write_be<uint48_t>(seconds_);
        buffer.write_be<uint32_t>(nanoseconds_);
    }

    /**
     * @return A string representation of the ptp_timestamp.
     */
    [[nodiscard]] std::string to_string() const {
        return fmt::format("{}.{:09}", seconds_, nanoseconds_);
    }

    /**
     * @return The seconds part of this timestamp (does not include the nanoseconds).
     */
    [[nodiscard]] uint64_t raw_seconds() const {
        return seconds_;
    }

    /**
     * @return The nanoseconds part of this timestamp (does not include the seconds).
     */
    [[nodiscard]] uint32_t raw_nanoseconds() const {
        return nanoseconds_;
    }

    /**
     * @return The total number of seconds (including fraction) represented by this timestamp as double.
     */
    [[nodiscard]] double to_seconds_double() const {
        return static_cast<double>(seconds_) + static_cast<double>(nanoseconds_) / 1'000'000'000.0;
    }

    /**
     * @return The number of nanoseconds represented by this timestamp. If the resulting number of nanoseconds is too
     * large to fit, the behaviour is undefined.
     */
    [[nodiscard]] uint64_t to_nanoseconds() const {
        return seconds_ * 1'000'000'000 + nanoseconds_;
    }

    /**
     * @return The number of milliseconds represented by this timestamp.
     */
    [[nodiscard]] double to_milliseconds_double() const {
        return static_cast<double>(seconds_) * 1'000.0 + static_cast<double>(nanoseconds_) / 1'000'000.0;
    }

    /**
     * @return The number of nanoseconds represented by this timestamp. The resulting number will clamp to int64 min/max
     * and never overflow.
     */
    [[nodiscard]] TimeInterval to_time_interval() const {
        RAV_ASSERT(nanoseconds_ < 1'000'000'000, "Nano seconds must be within [0, 1'000'000'000)");

        if (static_cast<int64_t>(seconds_) > std::numeric_limits<int64_t>::max()) {
            RAV_LOG_WARNING("Time interval overflow");
            return {};
        }

        return {static_cast<int64_t>(seconds_), static_cast<int32_t>(nanoseconds_), 0};
    }

    /**
     * Converts the timestamp to an RTP timestamp.
     * @param frequency The frequency, which is the sample rate for audio.
     * @return The RTP timestamp.
     */
    [[nodiscard]] uint64_t to_rtp_timestamp(const uint32_t frequency) const {
        RAV_ASSERT(seconds_ + 1 <= std::numeric_limits<decltype(seconds_)>::max() / frequency, "Overflow in seconds_ * sample_rate");
        return seconds_ * frequency + static_cast<uint64_t>(nanoseconds_) * frequency / 1'000'000'000;
    }

    /**
     * Converts an RTP timestamp to a PTP timestamp. The most significant bits of the current Timestamp are taken to form a full 64 bit
     * RTP timestamp, after which it's converted to a PTP timestamp.
     * @param rtp_timestamp The RTP timestamp.
     * @param frequency The frequency of the RTP time.
     * @return The reconstructed PTP timestamp.
     */
    [[nodiscard]] Timestamp from_rtp_timestamp(const uint32_t rtp_timestamp, const uint32_t frequency) const {
        const uint64_t ts_samples_full = (to_rtp_timestamp(frequency) & 0xFFFFFFFF00000000ULL) | rtp_timestamp;
        const auto seconds = static_cast<double>(ts_samples_full) / frequency;
        return Timestamp(static_cast<uint64_t>(seconds * 1'000'000'000.0));
    }

    /**
     * @return True if the timestamp is valid, false otherwise. The timestamp is considered valid when it is not zero.
     */
    [[nodiscard]] bool valid() const {
        return seconds_ != 0 || nanoseconds_ != 0;
    }

    /**
     * @return The current timestamp as an RFC 3339 string.
     */
    std::string to_rfc3339_tai() {
        if (seconds_ > static_cast<uint64_t>(std::numeric_limits<std::time_t>::max())) {
            return {};
        }
        const auto time = static_cast<std::time_t>(seconds_);

#if RAV_WINDOWS
        struct tm tm_result {};
        if (gmtime_s(&tm_result, &time) != 0) {
            return {};
        }
#else
        std::tm tm_result {};
        if (gmtime_r(&time, &tm_result) == nullptr) {
            return {};
        }
#endif

        return fmt::format(
            "{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:09}Z", tm_result.tm_year + 1900, tm_result.tm_mon + 1, tm_result.tm_mday,
            tm_result.tm_hour, tm_result.tm_min, tm_result.tm_sec, nanoseconds_
        );
    }

  private:
    uint64_t seconds_ {};      // 6 bytes (48 bits) on the wire
    uint32_t nanoseconds_ {};  // 4 bytes (32 bits) on the wire [0, 1'000'000'000).
};

}  // namespace rav::ptp
