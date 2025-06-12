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

#include "nmos_receiver_core.hpp"

namespace rav::nmos {

/**
 * Describes an audio Receiver.
 */
struct ReceiverAudio: ReceiverCore {
    static constexpr auto k_format = "urn:x-nmos:format:audio";

    struct Capabilities {
        /// Subclassifications of the format accepted, using IANA assigned media types where available, or other values
        /// defined in the NMOS Parameter Registers.
        /// Example: "audio/L24", audio/L20, "audio/L16", "audio/L8", "audio/PCM"
        std::vector<std::string> media_types;
    };

    /// Capabilities of the Receiver.
    Capabilities caps;

    /**
     * @return True if the receiver is valid, loosely following the NMOS JSON schema, or false otherwise.
     */
    [[nodiscard]] bool is_valid() const {
        if (id.is_nil()) {
            return false;
        }
        if (device_id.is_nil()) {
            return false;
        }
        if (caps.media_types.empty()) {
            return false;
        }
        for (const auto& media_type : caps.media_types) {
            if (!string_starts_with(media_type, "audio/")) {
                return false;
            }
        }
        return true;
    }
};

inline void tag_invoke(
    const boost::json::value_from_tag&, boost::json::value& jv, const ReceiverAudio::Capabilities& capabilities
) {
    jv = {
        {"media_types", boost::json::value_from(capabilities.media_types)},
    };
}

inline void tag_invoke(
    const boost::json::value_from_tag& tag, boost::json::value& jv, const ReceiverAudio& receiver
) {
    tag_invoke(tag, jv, static_cast<const ReceiverCore&>(receiver));
    auto& jv_obj = jv.as_object();
    jv_obj["format"] = ReceiverAudio::k_format;
    jv_obj["caps"] = boost::json::value_from(receiver.caps);
}

}  // namespace rav::nmos
