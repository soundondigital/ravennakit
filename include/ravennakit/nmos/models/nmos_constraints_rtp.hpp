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
    Constraint source_port;
    Constraint destination_port;
    Constraint destination_ip;
    Constraint rtp_enabled;
};

inline void
tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const ConstraintsRtp& constraint_rtp) {
    jv = {
        {"source_ip", boost::json::value_from(constraint_rtp.source_ip)},
        {"source_port", boost::json::value_from(constraint_rtp.source_ip)},
        {"destination_port", boost::json::value_from(constraint_rtp.destination_port)},
        {"destination_ip", boost::json::value_from(constraint_rtp.destination_port)},
        {"rtp_enabled", boost::json::value_from(constraint_rtp.rtp_enabled)},
    };
}

}  // namespace rav::nmos
