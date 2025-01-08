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

namespace rav {

/**
 * A PTP timestamp, consisting of a seconds and nanoseconds part.
 * Note: not suitable for memcpy to and from the wire.
 */
struct ptp_timestamp {
    uint64_t seconds {};      // 6 bytes (48 bits) on the wire
    uint32_t nanoseconds {};  // 4 bytes (32 bits) on the wire, should be 1'000'000'000 or less.

    /// Size on the wire in bytes.
    static constexpr size_t k_size = 10;

    ptp_timestamp() = default;

    /**
     * Create a ptp_timestamp from a number of nanoseconds.
     * @param nanos The number of nanoseconds.
     */
    explicit ptp_timestamp(const uint64_t nanos) {
        seconds = nanos / 1'000'000'000;
        nanoseconds = static_cast<uint32_t>(nanos % 1'000'000'000);
    }

    /**
     * Adds two ptp_timestamps together.
     * @param other The timestamp to add.
     * @return The sum of the two timestamps.
     */
    [[nodiscard]] ptp_timestamp operator+(const ptp_timestamp& other) const {
        auto result = *this;
        result.seconds += other.seconds;
        result.nanoseconds += other.nanoseconds;
        if (result.nanoseconds >= 1'000'000'000) {
            result.seconds += 1;
            result.nanoseconds -= 1'000'000'000;
        }
        return result;
    }

    /**
     * Subtracts two ptp_timestamps.
     * @param other The timestamp to subtract.
     * @return The difference of the two timestamps.
     */
    [[nodiscard]] ptp_timestamp operator-(const ptp_timestamp& other) const {
        auto result = *this;
        if (result.nanoseconds < other.nanoseconds) {
            result.seconds -= 1;
            result.nanoseconds += 1'000'000'000;
        }
        result.seconds -= other.seconds;
        result.nanoseconds -= other.nanoseconds;
        return result;
    }

    /**
     * Adds given time interval to the timestamp, returning the remaining fractional nanoseconds. This is
     * because ptp_timestamp has a resolution of 1 ns, but the correction field has a resolution of 1/65536 ns.
     * @param time_interval The correction field to subtract. This number is as specified in IEEE 1588-2019 13.3.2.9
     * @return The remaining fractional nanoseconds.
     */
    void add(const ptp_time_interval time_interval) {
        if (time_interval.seconds() < 0) {
            // Subtract the value
            const auto seconds_delta = static_cast<uint64_t>(std::abs(time_interval.seconds()));
            const auto nanoseconds_delta = static_cast<uint64_t>(std::abs(time_interval.nanos_raw()));
            if (nanoseconds_delta > nanoseconds) {
                if (seconds < 1) {
                    RAV_WARNING("ptp_timestamp underflow");
                    *this = {};  // Prevent underflow
                    return;
                }
                seconds -= 1;
                nanoseconds += 1'000'000'000;
            }
            if (seconds_delta > seconds) {
                RAV_WARNING("ptp_timestamp underflow");
                *this = {};  // Prevent underflow
                return;
            }
            seconds -= seconds_delta;
            nanoseconds -= nanoseconds_delta;
        } else {
            // Add the value
            const auto seconds_delta = static_cast<uint64_t>(time_interval.seconds());
            const auto nanoseconds_delta = static_cast<uint64_t>(time_interval.nanos_raw());
            seconds += seconds_delta;
            nanoseconds += nanoseconds_delta;
            if (nanoseconds >= 1'000'000'000) {
                seconds += 1;
                nanoseconds -= 1'000'000'000;
            }
        }
    }

    friend bool operator<(const ptp_timestamp& lhs, const ptp_timestamp& rhs) {
        return lhs.seconds < rhs.seconds || (lhs.seconds == rhs.seconds && lhs.nanoseconds < rhs.nanoseconds);
    }

    friend bool operator<=(const ptp_timestamp& lhs, const ptp_timestamp& rhs) {
        return rhs >= lhs;
    }

    friend bool operator>(const ptp_timestamp& lhs, const ptp_timestamp& rhs) {
        return rhs < lhs;
    }

    friend bool operator>=(const ptp_timestamp& lhs, const ptp_timestamp& rhs) {
        return !(lhs < rhs);
    }

    /**
     * Create a ptp_timestamp from a buffer_view. Data is assumed to valid, and the buffer_view must be at least 10
     * bytes long. No bounds checking is performed.
     * @param data The data to create the timestamp from. Assumed to be in network byte order.
     * @return The created ptp_timestamp.
     */
    static ptp_timestamp from_data(const buffer_view<const uint8_t> data) {
        RAV_ASSERT(data.size() >= 10, "data is too short to create a ptp_timestamp");
        ptp_timestamp ts;
        ts.seconds = data.read_be<uint48_t>(0).to_uint64();
        ts.nanoseconds = data.read_be<uint32_t>(6);
        return ts;
    }

    /**
     * Write the ptp_announce_message to a byte buffer.
     * @param buffer The buffer to write to.
     */
    void write_to(byte_buffer& buffer) const {
        buffer.write_be<uint48_t>(seconds);
        buffer.write_be<uint32_t>(nanoseconds);
    }

    /**
     * @return A string representation of the ptp_timestamp.
     */
    [[nodiscard]] std::string to_string() const {
        return fmt::format("{}.{:09}", seconds, nanoseconds);
    }

    /**
     * @return The number of nanoseconds represented by this timestamp. If the resulting number of nanoseconds is too
     * large to fit, the behaviour is undefined.
     */
    [[nodiscard]] uint64_t to_nanoseconds() const {
        return seconds * 1'000'000'000 + nanoseconds;
    }

    /**
     * @return The number of milliseconds represented by this timestamp.
     */
    [[nodiscard]] double to_milliseconds_double() const {
        return static_cast<double>(seconds) * 1'000.0 + static_cast<double>(nanoseconds) / 1'000'000.0;
    }

    /**
     * @return The number of nanoseconds represented by this timestamp. The resulting number will clamp to int64 min/max
     * and never overflow.
     */
    [[nodiscard]] ptp_time_interval to_time_interval() const {
        RAV_ASSERT(nanoseconds < 1'000'000'000, "Nano seconds must be within [0, 1'000'000'000)");

        const auto nanos = seconds * 1'000'000'000 + nanoseconds;

        if (seconds > std::numeric_limits<int64_t>::max()) {
            RAV_WARNING("Time interval overflow");
            return {};
        }

        return {static_cast<int64_t>(seconds), static_cast<int32_t>(nanos), 0};
    }
};

}  // namespace rav
