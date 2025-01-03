/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/messages/ptp_follow_up_message.hpp"

#include "ravennakit/ptp/messages/ptp_message_header.hpp"

tl::expected<rav::ptp_follow_up_message, rav::ptp_error>
rav::ptp_follow_up_message::from_data(const ptp_message_header& header, const buffer_view<const uint8_t> data) {
    if (data.size() < k_message_size) {
        return tl::unexpected(ptp_error::invalid_message_length);
    }

    ptp_follow_up_message msg;
    msg.header = header;
    msg.precise_origin_timestamp = ptp_timestamp::from_data(data);
    return msg;
}

std::string rav::ptp_follow_up_message::to_string() const {
    return fmt::format("precise_origin_timestamp={}", precise_origin_timestamp.to_string());
}
