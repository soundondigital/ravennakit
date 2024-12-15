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

#include "ravennakit/core/byte_order.hpp"
#include "ravennakit/core/containers/buffer_view.hpp"
#include "ravennakit/core/streams/output_stream.hpp"
#include "ravennakit/core/types/uint48.hpp"

namespace rav {

/**
 * A PTP timestamp, consisting of a seconds and nanoseconds part.
 * Note: due to padding not suitable for memcpy to and from the wire.
 */
struct ptp_timestamp {
    uint48_t seconds;
    uint32_t nanoseconds {};

    /// Size on the wire in bytes.
    static constexpr size_t k_size = 10;

    /**
     * Create a ptp_timestamp from a buffer_view. Data is assumed to valid, and the buffer_view must be at least 10
     * bytes long. No bounds checking is performed.
     * @param data The data to create the timestamp from. Assumed to be in network byte order.
     * @return The created ptp_timestamp.
     */
    static ptp_timestamp from_data(buffer_view<const uint8_t> data) {
        RAV_ASSERT(data.size() >= 10, "data is too short to create a ptp_timestamp");
        ptp_timestamp ts;
        ts.seconds = data.read_be<uint48_t>(0);
        ts.nanoseconds = data.read_be<uint32_t>(6);
        return ts;
    }

    /**
     * Writes the ptp_timestamp to the given stream.
     * @param stream The stream to write the ptp_timestamp to.
     */
    void write_to(output_stream& stream) const {
        stream.write_be<uint48_t>(seconds);
        stream.write_be<uint32_t>(nanoseconds);
    }

    /**
     * @return A string representation of the ptp_timestamp.
     */
    [[nodiscard]] std::string to_string() const {
        return fmt::format("{}.{:09}", seconds.to_uint64(), nanoseconds);
    }
};

using ptp_time_interval = int64_t;

}  // namespace rav
