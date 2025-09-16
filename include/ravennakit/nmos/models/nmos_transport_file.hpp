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

#include <optional>
#include <string>

namespace rav::nmos {

/**
 * Transport file parameters. 'data' and 'type' must both be strings or both be null. If 'type' is non-null 'data' is
 * expected to contain a valid instance of the specified media type."
 */
struct TransportFile {
    /// Content of the transport file
    std::string data;

    /// IANA assigned media type for file (e.g. application/sdp)
    std::string type;
};

inline void
tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const TransportFile& transport_file) {
    jv = {
        {"data", boost::json::value_from(transport_file.data)},
        {"type", boost::json::value_from(transport_file.type)},
    };
}

inline TransportFile
tag_invoke(const boost::json::value_to_tag<TransportFile>&, const boost::json::value& jv) {
    TransportFile transport_file;
    transport_file.data = jv.at("data").as_string();
    transport_file.type = jv.at("type").as_string();
    return transport_file;
}

}  // namespace rav::nmos