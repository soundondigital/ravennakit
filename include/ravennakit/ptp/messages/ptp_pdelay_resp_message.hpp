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
#include "ravennakit/core/streams/byte_stream.hpp"
#include "ravennakit/ptp/types/ptp_port_identity.hpp"
#include "ravennakit/ptp/types/ptp_timestamp.hpp"

namespace rav {

struct ptp_pdelay_resp_message {
    ptp_timestamp request_receipt_timestamp;
    ptp_port_identity requesting_port_identity;

    /**
     * Create a ptp_announce_message from a buffer_view.
     * @param data The message data. Expects it to start at the beginning of the message, excluding the header.
     * @return A ptp_announce_message if the data is valid, otherwise a ptp_error.
     */
    static tl::expected<ptp_pdelay_resp_message, ptp_error> from_data(buffer_view<const uint8_t> data);

    /**
     * @returns A string representation of the ptp_announce_message.
     */
    [[nodiscard]] std::string to_string() const;

private:
    constexpr static size_t k_message_size = 20; // Excluding header size
};

}
