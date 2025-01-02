/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/messages/ptp_sync_message.hpp"

tl::expected<rav::ptp_sync_message, rav::ptp_error>
rav::ptp_sync_message::from_data(const ptp_message_header& header, const buffer_view<const uint8_t> data) {
    if (data.size() < k_message_length) {
        return tl::unexpected(ptp_error::invalid_message_length);
    }

    ptp_sync_message msg;
    msg.header = header;
    msg.origin_timestamp = ptp_timestamp::from_data(data);
    return msg;
}

void rav::ptp_sync_message::write_to(output_stream& stream) const {
    header.write_to(stream);
    origin_timestamp.write_to(stream);
}

std::string rav::ptp_sync_message::to_string() const {
    return fmt::format("origin_timestamp={}", origin_timestamp.to_string());
}
