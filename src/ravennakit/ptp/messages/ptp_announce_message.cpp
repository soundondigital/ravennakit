/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/messages/ptp_announce_message.hpp"
#include "ravennakit/core/byte_order.hpp"

tl::expected<rav::ptp_announce_message, rav::ptp_error>
rav::ptp_announce_message::from_data(const buffer_view<const uint8_t> data) {
    auto header = ptp_message_header::from_data(data);
    if (!header) {
        return tl::unexpected(header.error());
    }

    ptp_announce_message announce;
    announce.header = header.value();

    const auto announce_data = data.subview(ptp_message_header::k_header_size);

    if (announce_data.size() < k_message_size) {
        return tl::unexpected(ptp_error::invalid_message_length);
    }

    announce.origin_timestamp.seconds = rav::byte_order::read_be<uint48_t>(data.data());
    announce.origin_timestamp.nanoseconds = rav::byte_order::read_be<uint32_t>(data.data() + 6);

    return announce;
}

std::string rav::ptp_announce_message::to_string() const {
    const auto txt = header.to_string();
    return txt
        + fmt::format(
               " origin_timestamp={}.{:09d}", origin_timestamp.seconds.to_uint64(), origin_timestamp.nanoseconds
        );
}
