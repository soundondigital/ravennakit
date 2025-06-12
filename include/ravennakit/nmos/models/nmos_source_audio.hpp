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
#include "nmos_source_core.hpp"

namespace rav::nmos {

/**
 * Describes an audio source.
 * https://specs.amwa.tv/is-04/releases/v1.3.3/APIs/schemas/with-refs/source_audio.html
 */
struct SourceAudio: SourceCore {
    static constexpr auto k_format = "urn:x-nmos:format:audio";

    struct Channel {
        /// Label for this channel (free text).
        std::string label;
    };

    /// Array of objects describing the audio channels
    std::vector<Channel> channels;

    /**
     * @return True if the receiver is valid, loosely following the NMOS JSON schema, or false otherwise.
     */
    bool is_valid() const {
        if (id.is_nil()) {
            return false;
        }
        if (channels.empty()) {
            return false;
        }
        return true;
    }
};

inline void
tag_invoke(const boost::json::value_from_tag&, boost::json::value& jv, const SourceAudio::Channel& channel) {
    jv = {
        {"label", boost::json::value_from(channel.label)},
    };
}

inline void tag_invoke(const boost::json::value_from_tag& tag, boost::json::value& jv, const SourceAudio& source) {
    tag_invoke(tag, jv, static_cast<const SourceCore&>(source));
    auto& obj = jv.as_object();
    obj["format"] = SourceAudio::k_format;
    obj["channels"] = boost::json::value_from(source.channels);
}

}  // namespace rav::nmos
