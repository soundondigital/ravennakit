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

#include "ravennakit/core/util.hpp"
#include "ravennakit/core/containers/buffer_view.hpp"
#include "ravennakit/ptp/ptp_profiles.hpp"
#include "ravennakit/ptp/messages/ptp_follow_up_message.hpp"
#include "ravennakit/ptp/messages/ptp_message.hpp"
#include "ravennakit/ptp/messages/ptp_pdelay_resp_follow_up_message.hpp"
#include "ravennakit/ptp/messages/ptp_pdelay_resp_message.hpp"

namespace {
const auto k_ptp_multicast_address = asio::ip::make_address("224.0.1.129");
constexpr auto k_ptp_event_port = 319;
constexpr auto k_ptp_general_port = 320;
}  // namespace

rav::ptp_port::ptp_port(
    asio::io_context& io_context, const asio::ip::address& interface_address, const ptp_port_identity port_identity
) :
    event_socket_(io_context, asio::ip::address_v4(), k_ptp_event_port),
    general_socket_(io_context, asio::ip::address_v4(), k_ptp_general_port) {
    // Initialize the port data set
    port_ds_.port_identity = port_identity;
    port_ds_.port_state = ptp_state::initializing;

    subscriptions_.push_back(event_socket_.join_multicast_group(k_ptp_multicast_address, interface_address));
    subscriptions_.push_back(general_socket_.join_multicast_group(k_ptp_multicast_address, interface_address));

    auto handler = [this](const udp_sender_receiver::recv_event& event) {
        handle_recv_event(event);
    };

    event_socket_.start(handler);
    general_socket_.start(handler);

    port_ds_.port_state = ptp_state::listening;
}

uint16_t rav::ptp_port::get_port_number() const {
    return port_ds_.port_identity.port_number;
}

void rav::ptp_port::assert_valid_state(const ptp_profile& profile) const {
    port_ds_.assert_valid_state(profile);
}

void rav::ptp_port::handle_recv_event(const udp_sender_receiver::recv_event& event) {
    const buffer_view data(event.data, event.size);
    auto header = ptp_message_header::from_data(data);
    if (!header) {
        RAV_TRACE("PTP Header error: {}", to_string(header.error()));
        return;
    }

    if (header->source_port_identity == port_ds_.port_identity) {
        RAV_WARNING("Received own message, ignoring");
        return;
    }

    switch (header->message_type) {
        case ptp_message_type::announce: {
            auto announce_message = ptp_announce_message::from_data(data.subview(ptp_message_header::k_header_size));
            if (!announce_message) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(announce_message.error()));
            }
            RAV_TRACE("{} {}", header->to_string(), announce_message->to_string());
            handle_announce_message(header.value(), announce_message.value(), {});
            break;
        }
        case ptp_message_type::sync: {
            auto sync_message = ptp_sync_message::from_data(data.subview(ptp_message_header::k_header_size));
            if (!sync_message) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(sync_message.error()));
            }
            RAV_TRACE("{} {}", header->to_string(), sync_message->to_string());
            handle_sync_message(header.value(), sync_message.value(), {});
            break;
        }
        case ptp_message_type::delay_req: {
            auto delay_req = ptp_delay_req_message::from_data(data.subview(ptp_message_header::k_header_size));
            if (!delay_req) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(delay_req.error()));
            }
            RAV_TRACE("{} {}", header->to_string(), delay_req->to_string());
            handle_delay_req_message(header.value(), delay_req.value(), {});
            break;
        }
        case ptp_message_type::p_delay_req: {
            auto pdelay_req = ptp_pdelay_req_message::from_data(data.subview(ptp_message_header::k_header_size));
            if (!pdelay_req) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(pdelay_req.error()));
            }
            RAV_TRACE("{} {}", header->to_string(), pdelay_req->to_string());
            handle_pdelay_req_message(header.value(), pdelay_req.value(), {});
            break;
        }
        case ptp_message_type::p_delay_resp: {
            auto pdelay_resp = ptp_pdelay_resp_message::from_data(data.subview(ptp_message_header::k_header_size));
            if (!pdelay_resp) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(pdelay_resp.error()));
            }
            RAV_TRACE("{} {}", header->to_string(), pdelay_resp->to_string());
            handle_pdelay_resp_message(header.value(), pdelay_resp.value(), {});
            break;
        }
        case ptp_message_type::follow_up: {
            auto follow_up = ptp_follow_up_message::from_data(data.subview(ptp_message_header::k_header_size));
            if (!follow_up) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(follow_up.error()));
            }
            RAV_TRACE("{} {}", header->to_string(), follow_up->to_string());
            handle_follow_up_message(header.value(), follow_up.value(), {});
            break;
        }
        case ptp_message_type::delay_resp: {
            auto delay_resp = ptp_delay_req_message::from_data(data.subview(ptp_message_header::k_header_size));
            if (!delay_resp) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(delay_resp.error()));
            }
            RAV_TRACE("{} {}", header->to_string(), delay_resp->to_string());
            handle_delay_resp_message(header.value(), delay_resp.value(), {});
            break;
        }
        case ptp_message_type::p_delay_resp_follow_up: {
            auto pdelay_resp_follow_up =
                ptp_pdelay_resp_follow_up_message::from_data(data.subview(ptp_message_header::k_header_size));
            if (!pdelay_resp_follow_up) {
                RAV_ERROR("{} error: {}", header->to_string(), to_string(pdelay_resp_follow_up.error()));
            }
            RAV_TRACE("{} {}", header->to_string(), pdelay_resp_follow_up->to_string());
            handle_pdelay_resp_follow_up_message(header.value(), pdelay_resp_follow_up.value(), {});
            break;
        }
        case ptp_message_type::signaling: {
            RAV_ASSERT_FALSE("Signaling messages are not implemented");
            break;
        }
        case ptp_message_type::management: {
            RAV_ASSERT_FALSE("Management messages are not implemented");
            break;
        }
        case ptp_message_type::reserved1:
        case ptp_message_type::reserved2:
        case ptp_message_type::reserved3:
        case ptp_message_type::reserved4:
        case ptp_message_type::reserved5:
        case ptp_message_type::reserved6:
        default: {
            RAV_WARNING("Unknown PTP message received: {}", to_string(header->message_type));
        }
    }
}

void rav::ptp_port::handle_announce_message(
    const ptp_message_header& header, const ptp_announce_message& announce_message, buffer_view<const uint8_t> tlvs
) {
    std::ignore = header;
    std::ignore = announce_message;
    std::ignore = tlvs;

    if (port_ds_.port_state == ptp_state::initializing) {
        RAV_TRACE("Discarding announce message while initializing");
        return;
    }

    if (port_ds_.port_state == ptp_state::disabled) {
        RAV_TRACE("Discarding announce message while disabled");
        return;
    }

    if (port_ds_.port_state == ptp_state::faulty) {
        RAV_TRACE("Discarding announce message while faulty");
        return;
    }


}

void rav::ptp_port::handle_sync_message(
    const ptp_message_header& header, const ptp_sync_message& sync_message, buffer_view<const uint8_t> tlvs
) {
    std::ignore = header;
    std::ignore = sync_message;
    std::ignore = tlvs;
}

void rav::ptp_port::handle_delay_req_message(
    const ptp_message_header& header, const ptp_delay_req_message& delay_req_message, buffer_view<const uint8_t> tlvs
) {
    std::ignore = header;
    std::ignore = delay_req_message;
    std::ignore = tlvs;
}

void rav::ptp_port::handle_follow_up_message(
    const ptp_message_header& header, const ptp_follow_up_message& follow_up_message, buffer_view<const uint8_t> tlvs
) {
    std::ignore = header;
    std::ignore = follow_up_message;
    std::ignore = tlvs;
}

void rav::ptp_port::handle_delay_resp_message(
    const ptp_message_header& header, const ptp_delay_req_message& delay_resp_message, buffer_view<const uint8_t> tlvs
) {
    std::ignore = header;
    std::ignore = delay_resp_message;
    std::ignore = tlvs;
}

void rav::ptp_port::handle_pdelay_req_message(
    const ptp_message_header& header, const ptp_pdelay_req_message& delay_req_message, buffer_view<const uint8_t> tlvs
) {
    std::ignore = header;
    std::ignore = delay_req_message;
    std::ignore = tlvs;
}

void rav::ptp_port::handle_pdelay_resp_message(
    const ptp_message_header& header, const ptp_pdelay_resp_message& delay_req_message, buffer_view<const uint8_t> tlvs
) {
    std::ignore = header;
    std::ignore = delay_req_message;
    std::ignore = tlvs;
}

void rav::ptp_port::handle_pdelay_resp_follow_up_message(
    const ptp_message_header& header, const ptp_pdelay_resp_follow_up_message& delay_req_message,
    buffer_view<const uint8_t> tlvs
) {
    std::ignore = header;
    std::ignore = delay_req_message;
    std::ignore = tlvs;
}
