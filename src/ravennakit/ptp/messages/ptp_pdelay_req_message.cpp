/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/messages/ptp_pdelay_req_message.hpp"

tl::expected<rav::ptp_pdelay_req_message, rav::ptp_error>
rav::ptp_pdelay_req_message::from_data(const buffer_view<const uint8_t> data) {
    if (data.size() < k_message_size) {
        return tl::make_unexpected(ptp_error::invalid_message_length);
    }

    ptp_pdelay_req_message msg;
    msg.origin_timestamp = ptp_timestamp::from_data(data);
    return msg;
}

void rav::ptp_pdelay_req_message::write_to(byte_buffer& buffer) const {
    return origin_timestamp.write_to(buffer);
}

std::string rav::ptp_pdelay_req_message::to_string() const {
    return fmt::format("origin_timestamp={}", origin_timestamp.to_string());
}
