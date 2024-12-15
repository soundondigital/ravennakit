/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/messages/ptp_pdelay_resp_follow_up_message.hpp"

tl::expected<rav::ptp_pdelay_resp_follow_up_message, rav::ptp_error>
rav::ptp_pdelay_resp_follow_up_message::from_data(const buffer_view<const uint8_t> data) {
    if (data.size() < k_message_size) {
        return tl::make_unexpected(ptp_error::invalid_message_length);
    }

    ptp_pdelay_resp_follow_up_message msg;
    msg.response_origin_timestamp = ptp_timestamp::from_data(data);
    auto port_identity = ptp_port_identity::from_data(data.subview(ptp_timestamp::k_size));
    if (!port_identity) {
        return tl::make_unexpected(port_identity.error());
    }
    msg.requesting_port_identity = port_identity.value();
    return msg;
}

void rav::ptp_pdelay_resp_follow_up_message::write_to(byte_stream& stream) const {
    response_origin_timestamp.write_to(stream);
    requesting_port_identity.write_to(stream);
}

std::string rav::ptp_pdelay_resp_follow_up_message::to_string() const {
    return fmt::format(
        "response_origin_timestamp={} requesting_port_identity={}", response_origin_timestamp.to_string(),
        requesting_port_identity.to_string()
    );
}
