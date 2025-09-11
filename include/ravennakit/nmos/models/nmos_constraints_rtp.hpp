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

#include "nmos_constraint.hpp"

namespace rav::nmos {

/**
 * Used to express the dynamic constraints on RTP transport parameters. These constraints may be set and changed at run
 * time. Every transport parameter must have an entry, even if it is only an empty object.
 */
struct ConstraintsRtp {
    Constraint source_ip;
    Constraint destination_port;
    Constraint rtp_enabled;
    std::optional<Constraint> source_port;     // Required for senders
    std::optional<Constraint> destination_ip;  // Required for senders
    std::optional<Constraint> interface_ip;    // Required for receivers
    std::optional<Constraint> multicast_ip;    // Required for receivers if supported
};

inline void
tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const ConstraintsRtp& constraint_rtp) {
    boost::json::object obj {
        {"source_ip", boost::json::value_from(constraint_rtp.source_ip)},
        {"destination_port", boost::json::value_from(constraint_rtp.destination_port)},
        {"rtp_enabled", boost::json::value_from(constraint_rtp.rtp_enabled)},
    };

    if (constraint_rtp.source_port.has_value()) {
        obj["source_port"] = boost::json::value_from(constraint_rtp.source_port);
    }
    if (constraint_rtp.destination_ip.has_value()) {
        obj["destination_ip"] = boost::json::value_from(constraint_rtp.destination_ip);
    }
    if (constraint_rtp.interface_ip.has_value()) {
        obj["interface_ip"] = boost::json::value_from(constraint_rtp.interface_ip);
    }
    if (constraint_rtp.multicast_ip.has_value()) {
        obj["multicast_ip"] = boost::json::value_from(constraint_rtp.multicast_ip);
    }

    jv = std::move(obj);
}

}  // namespace rav::nmos
