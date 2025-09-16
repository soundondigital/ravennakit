/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/core/json.hpp"

namespace rav::nmos {

struct ReceiverTransportParamsRtp {
    /**
     * Source IP address of RTP packets in unicast mode, or source filter for source specific multicast. A null value
     * indicates that the source IP address has not been configured in unicast mode, or the Receiver is in any-source
     * multicast mode.
     */
    std::optional<std::string> source_ip;

    /**
     * IP address of the network interface the receiver should use. The receiver should provide an enum in the
     * constraints endpoint, which should contain the available interface addresses. If set to auto in multicast mode
     * the receiver should determine which interface to use for itself, for example by using the routing tables. The
     * behaviour of auto is undefined in unicast mode, and controllers should supply a specific interface address.
     */
    std::optional<std::string> interface_ip {"auto"};

    /**
     * RTP reception active/inactive.
     */
    std::optional<bool> rtp_enabled {};

    /**
     * Destination port for RTP packets (auto = 5004 by default)
     */
    std::variant<std::monostate, int, std::string> destination_port {"auto"};

    /**
     * IP multicast group address used in multicast operation only. Should be set to null during unicast operation. A
     * null value indicates the parameter has not been configured, or the receiver is operating in unicast mode.
     */
    std::optional<std::string> multicast_ip;
};

inline void tag_invoke(
    const boost::json::value_from_tag&, boost::json::value& jv, const ReceiverTransportParamsRtp& transport_file
) {
    jv = {
        {"source_ip", boost::json::value_from(transport_file.source_ip)},
        {"interface_ip", boost::json::value_from(transport_file.interface_ip)},
        {"rtp_enabled", boost::json::value_from(transport_file.rtp_enabled)},
        {"destination_port", boost::json::value_from(transport_file.destination_port)},
        {"multicast_ip", boost::json::value_from(transport_file.multicast_ip)},
    };
}

inline ReceiverTransportParamsRtp
tag_invoke(const boost::json::value_to_tag<ReceiverTransportParamsRtp>&, const boost::json::value& jv) {
    ReceiverTransportParamsRtp transport_params_rtp{};

    if (const auto result = jv.try_at("source_ip")) {
        transport_params_rtp.source_ip = boost::json::value_to<std::optional<std::string>>(*result);
    }

    if (const auto result = jv.try_at("interface_ip")) {
        transport_params_rtp.interface_ip = boost::json::value_to<std::optional<std::string>>(*result);
    }

    if (const auto result = jv.try_at("rtp_enabled")) {
        transport_params_rtp.rtp_enabled = boost::json::value_to<std::optional<bool>>(*result);
    }

    if (const auto result = jv.try_at("destination_port")) {
        if (result->is_string()) {
            transport_params_rtp.destination_port = std::string(result->get_string());
        } else if (result->is_number()) {
            transport_params_rtp.destination_port = result->to_number<uint16_t>();
        } else {
            throw std::runtime_error("Invalid type");
        }
    }

    if (const auto result = jv.try_at("multicast_ip")) {
        transport_params_rtp.multicast_ip = boost::json::value_to<std::optional<std::string>>(*result);
    }

    return transport_params_rtp;
}

}  // namespace rav::nmos
