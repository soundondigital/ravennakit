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

#include <string>

namespace rav::nmos {

/**
 * Describes RTP Sender transport parameters. The constraints in this schema are minimum constraints, but may be further
 * constrained at the constraints' endpoint. As a minimum all senders must support `source_ip`, `destination_ip`,
 * `source_port`, `rtp_enabled` and `destination_port`. Senders supporting FEC and/or RTCP must support parameters
 * prefixed with `fec` and `rtcp` respectively.
 */
struct SenderTransportParamsRtp {
    /**
     * IP address from which RTP packets will be sent (IP address of interface bound to this output). The sender should
     * provide an enum in the constraints endpoint, which should contain the available interface addresses. If the
     * parameter is set to auto the sender should establish for itself which interface it should use, based on routing
     * rules or its own internal configuration.
     */
    std::string source_ip {"auto"};

    /**
     * IP address to which RTP packets will be sent. If auto is set the sender should select a multicast address to send
     * to itself. For example, it may implement MADCAP (RFC 2730), ZMAAP, or be allocated address by some other system
     * responsible for co-ordination multicast address use.
     */
    std::string destination_ip {"auto"};

    /**
     * Source port for RTP packets (auto = 5004 by default)
     */
    std::variant<int, std::string> source_port {"auto"};

    /**
     * destination port for RTP packets (auto = 5004 by default)
     */
    std::variant<int, std::string> destination_port {"auto"};

    /**
     * RTP transmission active/inactive
     */
    bool rtp_enabled {false};
};

inline void tag_invoke(
    const boost::json::value_from_tag&, boost::json::value& jv, const SenderTransportParamsRtp& sender_transport_params
) {
    jv = {
        {"source_ip", boost::json::value_from(sender_transport_params.source_ip)},
        {"destination_ip", boost::json::value_from(sender_transport_params.destination_ip)},
        {"source_port", boost::json::value_from(sender_transport_params.source_port)},
        {"destination_port", boost::json::value_from(sender_transport_params.destination_port)},
        {"rtp_enabled", boost::json::value_from(sender_transport_params.rtp_enabled)},
    };
}

}  // namespace rav::nmos
