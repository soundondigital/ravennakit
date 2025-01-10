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
    /// Size on the wire in bytes.
    static constexpr size_t k_size = 10;

    ptp_timestamp() = default;

    /**
     * Create a ptp_timestamp from a number of nanoseconds.
     * @param nanos The number of nanoseconds.
     */
    explicit ptp_timestamp(const uint64_t nanos) {
        seconds_ = nanos / 1'000'000'000;
        nanoseconds_ = static_cast<uint32_t>(nanos % 1'000'000'000);
    }

    /**
     * Create a ptp_timestamp from seconds and milliseconds.
     * @param seconds The number of seconds.
     * @param nanoseconds The number of nanoseconds.
     */
    ptp_timestamp(const uint64_t seconds, const uint32_t nanoseconds) :
        seconds_(seconds), nanoseconds_(nanoseconds) {}

    /**
     * Adds two ptp_timestamps together.
     * @param other The timestamp to add.
     * @return The sum of the two timestamps.
     */
    [[nodiscard]] ptp_time_interval operator+(const ptp_timestamp& other) const {
        return this->to_time_interval() + other.to_time_interval();
    }

    /**
     * Subtracts two ptp_timestamps.
     * @param other The timestamp to subtract.
     * @return The difference of the two timestamps.
     */
    [[nodiscard]] ptp_time_interval operator-(const ptp_timestamp& other) const {
        return this->to_time_interval() - other.to_time_interval();
    }

    /**
     * Adds given time interval to the timestamp.
     * @param time_interval The correction field to subtract. This number is as specified in IEEE 1588-2019 13.3.2.9
     * @return The remaining fractional nanoseconds.
     */
    void add(const ptp_time_interval time_interval) {
        if (time_interval.seconds() < 0) {
            // Subtract the value
            const auto seconds_delta = static_cast<uint64_t>(std::abs(time_interval.seconds()));
            const auto nanoseconds_delta = static_cast<uint64_t>(std::abs(time_interval.nanos()));
            if (nanoseconds_delta > nanoseconds_) {
                if (seconds_ < 1) {
                    RAV_WARNING("ptp_timestamp underflow");
                    *this = {};  // Prevent underflow
                    return;
                }
                seconds_ -= 1;
                nanoseconds_ += 1'000'000'000;
            }
            if (seconds_delta > seconds_) {
                RAV_WARNING("ptp_timestamp underflow");
                *this = {};  // Prevent underflow
                return;
            }
            seconds_ -= seconds_delta;
            nanoseconds_ -= nanoseconds_delta;
        } else {
            // Add the value
            const auto seconds_delta = static_cast<uint64_t>(time_interval.seconds());
            const auto nanoseconds_delta = static_cast<uint64_t>(time_interval.nanos());
            seconds_ += seconds_delta;
            nanoseconds_ += nanoseconds_delta;
            if (nanoseconds_ >= 1'000'000'000) {
                seconds_ += 1;
                nanoseconds_ -= 1'000'000'000;
            }
        }
    }

    friend bool operator<(const ptp_timestamp& lhs, const ptp_timestamp& rhs) {
        return lhs.seconds_ < rhs.seconds_ || (lhs.seconds_ == rhs.seconds_ && lhs.nanoseconds_ < rhs.nanoseconds_);
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
        ts.seconds_ = data.read_be<uint48_t>(0).to_uint64();
        ts.nanoseconds_ = data.read_be<uint32_t>(6);
        return ts;
    }

    /**
     * Write the ptp_announce_message to a byte buffer.
     * @param buffer The buffer to write to.
     */
    void write_to(byte_buffer& buffer) const {
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
     * @return The number of seconds represented by this timestamp (does not include the nanoseconds).
     */
    [[nodiscard]] uint64_t seconds() const {
        return seconds_;
    }

    /**
     * @return The number of nanoseconds represented by this timestamp (does not include the seconds).
     */
    [[nodiscard]] uint32_t nanoseconds() const {
        return nanoseconds_;
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
    [[nodiscard]] ptp_time_interval to_time_interval() const {
        RAV_ASSERT(nanoseconds_ < 1'000'000'000, "Nano seconds must be within [0, 1'000'000'000)");

        if (seconds_ > std::numeric_limits<int64_t>::max()) {
            RAV_WARNING("Time interval overflow");
            return {};
        }

        return {static_cast<int64_t>(seconds_), static_cast<int32_t>(nanoseconds_), 0};
    }

  private:
    uint64_t seconds_ {};      // 6 bytes (48 bits) on the wire
    uint32_t nanoseconds_ {};  // 4 bytes (32 bits) on the wire, should be 1'000'000'000 or less.
};

}  // namespace rav
