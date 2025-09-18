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

#include "nmos_api_error.hpp"
#include "nmos_resource_core.hpp"

#include "ravennakit/core/util/safe_function.hpp"
#include "ravennakit/nmos/detail/nmos_uuid.hpp"
#include "ravennakit/sdp/sdp.hpp"

namespace rav::nmos {

/**
 * Describes a receiver.
 */
struct ReceiverCore: ResourceCore {
    /**
     * Object indicating how this Receiver is currently configured to receive data.
     */
    struct Subscription {
        /// UUID of the Sender from which this Receiver is currently configured to receive data. Only set if it is
        /// active and receiving from an NMOS Sender; otherwise null.
        std::optional<boost::uuids::uuid> sender_id;

        /// Whether the Receiver is enabled and configured to receive data.
        bool active {false};
    };

    /// Device ID which this Receiver forms part of. This attribute is used to ensure referential integrity by registry
    /// implementations.
    boost::uuids::uuid device_id;

    /// Transport type accepted by the Receiver in URN format.
    /// For example, "urn:x-nmos:transport:rtp", "urn:x-nmos:transport:rtp.mcast"
    /// Look for the list of registered transports in the NMOS registry:
    /// https://specs.amwa.tv/nmos-parameter-registers/branches/main/transports/
    std::string transport;

    /// Binding of Receiver ingress ports to interfaces on the parent Node.
    std::vector<std::string> interface_bindings;

    /// Object indicating how this Receiver is currently configured to receive data.
    Subscription subscription;

    std::function<tl::expected<void, ApiError>(const boost::json::value& patch_request)> on_patch_request;
    std::function<tl::expected<sdp::SessionDescription, ApiError>()> get_transport_file;
};

inline void
tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const ReceiverCore::Subscription& subscription) {
    auto object = boost::json::object {
        {"active", subscription.active},
    };
    if (subscription.sender_id.has_value()) {
        object["sender_id"] = boost::uuids::to_string(*subscription.sender_id);
    } else {
        object["sender_id"] = nullptr;
    }
    jv = object;
}

inline ReceiverCore::Subscription
tag_invoke(const boost::json::value_to_tag<ReceiverCore::Subscription>&, const boost::json::value& jv) {
    ReceiverCore::Subscription sub;
    sub.sender_id = uuid_from_json(jv.at("sender_id"));
    sub.active = jv.at("active").as_bool();
    return sub;
}

inline void tag_invoke(const boost::json::value_from_tag& tag, boost::json::value& jv, const ReceiverCore& receiver) {
    tag_invoke(tag, jv, static_cast<const ResourceCore&>(receiver));
    auto& jv_obj = jv.as_object();
    jv_obj["device_id"] = boost::uuids::to_string(receiver.device_id);
    jv_obj["transport"] = receiver.transport;
    jv_obj["interface_bindings"] = boost::json::value_from(receiver.interface_bindings);
    jv_obj["subscription"] = boost::json::value_from(receiver.subscription);
}

}  // namespace rav::nmos
