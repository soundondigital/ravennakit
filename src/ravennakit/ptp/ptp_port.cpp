/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/ptp_port.hpp"

#include "ravennakit/core/containers/buffer_view.hpp"
#include "ravennakit/ptp/messages/ptp_message.hpp"

namespace {
const auto k_ptp_multicast_address = asio::ip::make_address("224.0.1.129");
constexpr auto k_ptp_event_port = 319;
constexpr auto k_ptp_general_port = 320;
}  // namespace

rav::ptp_port::ptp_port(
    asio::io_context& io_context, const asio::ip::address& interface_address, ptp_port_identity port_identity
) :
    event_socket_(io_context, asio::ip::address_v4(), k_ptp_event_port),
    general_socket_(io_context, asio::ip::address_v4(), k_ptp_general_port) {
    port_ds_.port_identity = port_identity;
    port_ds_.port_state = ptp_state::initializing;
    subscriptions_.push_back(event_socket_.join_multicast_group(k_ptp_multicast_address, interface_address));
    subscriptions_.push_back(general_socket_.join_multicast_group(k_ptp_multicast_address, interface_address));

    auto handler = [](const udp_sender_receiver::recv_event& event) {
        const buffer_view data(event.data, event.size);
        auto header = ptp_message_header::from_data(data);
        if (!header) {
            RAV_TRACE("PTP Error: {}", static_cast<std::underlying_type_t<rav::ptp_error>>(header.error()));
            return;
        }
        RAV_TRACE("{}", header->to_string());
    };

    event_socket_.start(handler);
    general_socket_.start(handler);
}
