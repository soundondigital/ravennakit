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
#include "ravennakit/ptp/ptp_error.hpp"
#include "ravennakit/ptp/types/ptp_port_identity.hpp"
#include "ravennakit/ptp/types/ptp_timestamp.hpp"

#include <tl/expected.hpp>

namespace rav {

struct ptp_delay_resp_message {
    ptp_message_header header;
    ptp_timestamp receive_timestamp;
    ptp_port_identity requesting_port_identity;

    /**
     * Create a ptp_announce_message from a buffer_view.
     * @param header The message header belonging to the message.
     * @param data The message data. Expects it to start at the beginning of the message, excluding the header.
     * @return A ptp_announce_message if the data is valid, otherwise a ptp_error.
     */
    static tl::expected<ptp_delay_resp_message, ptp_error>
    from_data(const ptp_message_header& header, buffer_view<const uint8_t> data);

    /**
     * @returns A string representation of the ptp_announce_message.
     */
    [[nodiscard]] std::string to_string() const;

  private:
    constexpr static size_t k_message_size = 20;  // Excluding header size
};

}  // namespace rav
