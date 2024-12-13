/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/messages/ptp_message.hpp"

tl::expected<rav::ptp_message, rav::ptp_error> rav::ptp_message::from_data(const buffer_view<const uint8_t> data) {
    auto header = ptp_message_header::from_data(data);
    if (!header) {
        return tl::unexpected(header.error());
    }

    ptp_message message;
    return message;
}

std::string rav::ptp_message::to_string() const {
    return {};
}
