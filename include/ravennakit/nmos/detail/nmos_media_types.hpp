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

#include "ravennakit/core/audio/audio_format.hpp"

namespace rav::nmos {

inline const char* audio_format_to_nmos_media_type(const AudioFormat& format) {
    switch (format.encoding) {
        case AudioEncoding::undefined:
            return "audio/undefined";
        case AudioEncoding::pcm_s8:
            return "audio/L8";
        case AudioEncoding::pcm_u8:
            return "audio/U8";  // Non-standard
        case AudioEncoding::pcm_s16:
            return "audio/L16";
        case AudioEncoding::pcm_s24:
            return "audio/L24";
        case AudioEncoding::pcm_s32:
            return "audio/L32";
        case AudioEncoding::pcm_f32:
            return "audio/F32";
        case AudioEncoding::pcm_f64:
            return "audio/F64";
        default:
            return "audio/unknown";
    }
}

}
