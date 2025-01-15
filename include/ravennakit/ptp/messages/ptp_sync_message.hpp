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
#include "ravennakit/core/containers/byte_buffer.hpp"
#include "ravennakit/ptp/types/ptp_timestamp.hpp"

namespace rav {

struct ptp_sync_message {
    constexpr static size_t k_message_length = ptp_message_header::k_header_size + 10;

    ptp_message_header header;
    ptp_timestamp origin_timestamp;

    /**
     * Create a ptp_announce_message from a buffer_view.
     * @param header The header of the message.
     * @param data The message data. Expects it to start at the beginning of the message, excluding the header.
     * @return A ptp_announce_message if the data is valid, otherwise a ptp_error.
     */
    static tl::expected<ptp_sync_message, ptp_error> from_data(const ptp_message_header& header, buffer_view<const uint8_t> data);

    /**
     * Write the ptp_announce_message to a byte buffer.
     * @param buffer The buffer to write to.
     */
    void write_to(byte_buffer& buffer) const;

    /**
     * @returns A string representation of the ptp_announce_message.
     */
    [[nodiscard]] std::string to_string() const;
};

}
