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

#include "nmos_resource.hpp"

#include <boost/uuid.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value_from.hpp>

namespace rav::nmos {

struct Device: Resource {
    static constexpr auto k_type_generic = "urn:x-nmos:device:generic";
    static constexpr auto k_type_pipeline = "urn:x-nmos:device:pipeline";

    struct Control {
        /// URL to reach a control endpoint, whether http or otherwise
        std::string href;

        /// URN identifying the control format
        std::string type;

        /// Whether this endpoint requires authorization, not required
        std::optional<bool> authorization;
    };

    /// Device type URN (urn:x-nmos:device:<type>)
    std::string type {k_type_generic};

    /// Globally unique identifier for the Node which initially created the Device. This attribute is used to ensure
    /// referential integrity by registry implementations.
    boost::uuids::uuid node_id;

    /// Control endpoints exposed for the Device
    std::vector<Control> controls;
};

inline void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const Device::Control& control) {
    jv = {{"href", control.href}, {"type", control.type}};
    if (control.authorization) {
        jv.as_object()["authorization"] = *control.authorization;
    }
}

inline void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const Device& device) {
    jv = {
        {"id", to_string(device.id)},
        {"version", device.version.to_string()},
        {"label", device.label},
        {"description", device.description},
        {"tags", boost::json::value_from(device.tags)},
        {"type", device.type},
        {"node_id", to_string(device.node_id)},
        {"controls", boost::json::value_from(device.controls)},
        {"receivers", boost::json::array()},  // TODO: Add receivers
        {"senders", boost::json::array()},    // TODO: Add senders
    };
}

}  // namespace rav::nmos
