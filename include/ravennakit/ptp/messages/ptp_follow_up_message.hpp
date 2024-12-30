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
#include "ravennakit/ptp/types/ptp_timestamp.hpp"

#include <tl/expected.hpp>

namespace rav {

struct ptp_follow_up_message {
    ptp_message_header header;
    ptp_timestamp precise_origin_timestamp;

    /**
     * Create a ptp_announce_message from a buffer_view.
     * @param header The header of the message.
     * @param data The message data. Expects it to start at the beginning of the message, excluding the header.
     * @return A ptp_announce_message if the data is valid, otherwise a ptp_error.
     */
    static tl::expected<ptp_follow_up_message, ptp_error>
    from_data(const ptp_message_header& header, buffer_view<const uint8_t> data);

    /**
     * Write the ptp_announce_message to a stream.
     * @param stream The stream to write to.
     */
    void write_to(output_stream& stream) const;

    /**
     * @returns A string representation of the ptp_announce_message.
     */
    [[nodiscard]] std::string to_string() const;

  private:
    constexpr static size_t k_message_size = 10;  // Excluding header size
};

}  // namespace rav
