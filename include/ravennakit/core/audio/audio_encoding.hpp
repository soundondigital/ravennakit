/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once
#include <cstdint>

namespace rav {

enum class audio_encoding : uint8_t {
    undefined,
    pcm_s8,
    pcm_u8,
    pcm_s16,
    pcm_s24,
    pcm_s32,
    pcm_f32,
    pcm_f64,
};

/**
 * @return The number of bytes per sample.
 */
inline uint8_t audio_encoding_bytes_per_sample(const audio_encoding encoding) {
    switch (encoding) {
        case audio_encoding::pcm_s8:
        case audio_encoding::pcm_u8:
            return 1;
        case audio_encoding::pcm_s16:
            return 2;
        case audio_encoding::pcm_s24:
            return 3;
        case audio_encoding::pcm_s32:
        case audio_encoding::pcm_f32:
            return 4;
        case audio_encoding::pcm_f64:
            return 8;
        case audio_encoding::undefined:
        default:
            return 0;
    }
}

/**
 * @return The ground value for the encoding.
 */
inline uint8_t audio_encoding_ground_value(const audio_encoding encoding) {
    switch (encoding) {
        case audio_encoding::pcm_u8:
            return 0x80;
        case audio_encoding::pcm_s8:
        case audio_encoding::pcm_s16:
        case audio_encoding::pcm_s24:
        case audio_encoding::pcm_s32:
        case audio_encoding::pcm_f32:
        case audio_encoding::pcm_f64:
        case audio_encoding::undefined:
        default:
            return 0;
    }
}

inline const char* audio_encoding_to_string(const audio_encoding encoding) {
    switch (encoding) {
        case audio_encoding::undefined:
            return "undefined";
        case audio_encoding::pcm_s8:
            return "pcm_s8";
        case audio_encoding::pcm_u8:
            return "pcm_u8";
        case audio_encoding::pcm_s16:
            return "pcm_s16";
        case audio_encoding::pcm_s24:
            return "pcm_s24";
        case audio_encoding::pcm_s32:
            return "pcm_s32";
        case audio_encoding::pcm_f32:
            return "pcm_f32";
        case audio_encoding::pcm_f64:
            return "pcm_f64";
        default:
            return "unknown";
    }
}

}  // namespace rav
