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
#include "nmos_flow_audio.hpp"

namespace rav::nmos {

struct FlowAudioRaw: FlowAudio {
    /// Subclassification of the format, using IANA assigned media types where available, or other values defined in the
    /// NMOS Parameter Registers.
    std::string media_type {};  // audio/L24, audio/L20, audio/L16, audio/L8

    /// Bit depth of the audio samples.
    int bit_depth {};           // 8, 16, 20, 24

    /**
     * @return True if the flow is valid, loosely following the NMOS JSON schema, or false otherwise.
     */
    [[nodiscard]] bool is_valid() const {
        if (id.is_nil()) {
            return false;
        }
        if (media_type.empty()) {
            return false;
        }
        if (bit_depth <= 0) {
            return false;
        }
        if (sample_rate.numerator <= 0 || sample_rate.denominator <= 0) {
            return false;
        }
        return true;
    }
};

inline void tag_invoke(
    const boost::json::value_from_tag& tag, boost::json::value& jv, const FlowAudioRaw& flow_audio_raw
) {
    tag_invoke(tag, jv, static_cast<const FlowAudio&>(flow_audio_raw));
    auto& object = jv.as_object();
    object["media_type"] = flow_audio_raw.media_type;
    object["bit_depth"] = flow_audio_raw.bit_depth;
}

}  // namespace rav::nmos
