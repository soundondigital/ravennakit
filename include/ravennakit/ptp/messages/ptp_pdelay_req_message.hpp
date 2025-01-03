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

#include "ravennakit/ptp/ptp_error.hpp"
#include "ravennakit/ptp/types/ptp_timestamp.hpp"

#include <tl/expected.hpp>

namespace rav {

struct ptp_pdelay_req_message {
    // TODO: Add the header
    ptp_timestamp origin_timestamp;
    const uint8_t reserved[10] = {}; // To match the messages length of the pdelay_resp message.

    /**
     * Create a ptp_announce_message from a buffer_view.
     * @param data The message data. Expects it to start at the beginning of the message, excluding the header.
     * @return A ptp_announce_message if the data is valid, otherwise a ptp_error.
     */
    static tl::expected<ptp_pdelay_req_message, ptp_error> from_data(buffer_view<const uint8_t> data);

    /**
     * Write the ptp_announce_message to a byte buffer.
     * @param buffer The buffer to write to.
     */
    void write_to(byte_buffer& buffer) const;

    /**
     * @returns A string representation of the ptp_announce_message.
     */
    [[nodiscard]] std::string to_string() const;

private:
    constexpr static size_t k_message_size = 20; // Excluding header size
};

}
