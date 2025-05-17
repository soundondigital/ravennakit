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

#include "nmos_clock_internal.hpp"
#include "nmos_clock_ptp.hpp"
#include "nmos_resource.hpp"

namespace rav::nmos {

/**
 * Describes the Node and the services which run on it.
 */
struct Self: Resource {
    struct Endpoint {
        /// IP address or hostname which the Node API is running on
        std::string host;

        /// Port number which the Node API is running on
        uint16_t port;

        /// Protocol supported by this instance of the Node API
        std::string protocol;  // e.g. "http", "https"

        /// This endpoint requires authorization (optional)
        bool authorization;
    };

    struct Api {
        /// Supported API versions running on this Node
        std::vector<std::string> versions;

        /// Host, port and protocol details required to connect to the API
        std::vector<Endpoint> endpoints;
    };

    struct Interface {
        /// Chassis ID of the interface, as signaled in LLDP from this node. Set to null where LLDP is unsuitable for
        /// use (i.e., virtualized environments).
        std::optional<std::string> chassis_id;

        /// Port ID of the interface, as signaled in LLDP or via ARP responses from this node. Must be a MAC address
        /// (e.g. "00-1a-2b-3c-4d-5e").
        std::string port_id;

        /// Name of the interface (unique in the scope of this node).  This attribute is used by sub-resources of this
        /// node such as senders and receivers to refer to interfaces to which they are bound.
        std::string name;
    };

    /// HTTP access href for the Node's API (deprecated)
    std::string href;

    /// URL fragments required to connect to the Node API
    Api api;

    /// Clocks made available to Devices owned by this Node
    std::vector<std::variant<ClockInternal, ClockPtp>> clocks;

    /// Network interfaces made available to devices owned by this Node. Port IDs and Chassis IDs are used to inform
    /// topology discovery via IS-06 and require that interfaces implement ARP at a minimum, and ideally LLDP.
    std::vector<Interface> interfaces;
};

inline void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const Self::Endpoint& endpoint) {
    jv = {
        {"host", endpoint.host},
        {"port", endpoint.port},
        {"protocol", endpoint.protocol},
        {"authorization", endpoint.authorization}
    };
}

inline void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const Self::Api& api) {
    jv = {{"versions", boost::json::value_from(api.versions)}, {"endpoints", boost::json::value_from(api.endpoints)}};
}

inline void tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const Self::Interface& interface) {
    jv = {
        {"chassis_id", boost::json::value_from(interface.chassis_id)},
        {"port_id", interface.port_id},
        {"name", interface.name}
    };
}

inline void tag_invoke(const boost::json::value_from_tag& tag, boost::json::value& jv, const Self& self) {
    tag_invoke(tag, jv, static_cast<const Resource&>(self));
    auto& object = jv.as_object();
    object["href"] = self.href;
    object["caps"] = boost::json::object();
    object["api"] = boost::json::value_from(self.api);
    object["services"] = boost::json::array();
    object["clocks"] = boost::json::value_from(self.clocks);
    object["interfaces"] = boost::json::value_from(self.interfaces);
}

}  // namespace rav::nmos
