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

#include "ptp_message_header.hpp"
#include "ravennakit/ptp/types/ptp_clock_quality.hpp"
#include "ravennakit/ptp/types/ptp_timestamp.hpp"

namespace rav {

struct ptp_announce_message {
    ptp_timestamp origin_timestamp;
    int16_t current_utc_offset {}; // Seconds
    uint8_t grandmaster_priority1 {};
    ptp_clock_quality grandmaster_clock_quality;
    uint8_t grandmaster_priority2 {};
    ptp_clock_identity grandmaster_identity;
    uint16_t steps_removed {};
    ptp_time_source time_source{};

    /**
     * Create a ptp_announce_message from a buffer_view.
     * @param data The message data. Expects it to start at the beginning of the message, excluding the header.
     * @return A ptp_announce_message if the data is valid, otherwise a ptp_error.
     */
    static tl::expected<ptp_announce_message, ptp_error> from_data(buffer_view<const uint8_t> data);

    /**
     * Writes the ptp_announce_message to the given stream.
     * @param stream The stream to write the ptp_announce_message to.
     */
    void write_to(output_stream& stream) const;

    /**
     * @returns A string representation of the ptp_announce_message.
     */
    [[nodiscard]] std::string to_string() const;
private:
    constexpr static size_t k_message_size = 30; // Excluding header size
};

}
