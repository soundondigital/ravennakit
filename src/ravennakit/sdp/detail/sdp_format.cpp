/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_format.hpp"
#include "ravennakit/core/string_parser.hpp"

#include "ravennakit/core/format.hpp"

std::string rav::sdp::format::to_string() const {
    return fmt::format("{} {}/{}/{}", payload_type, encoding_name, clock_rate, num_channels);
}

std::optional<rav::audio_format> rav::sdp::format::to_audio_format() const {
    if (encoding_name == "L16") {
        return audio_format {
            audio_format::byte_order::be, audio_encoding::pcm_s16, clock_rate, num_channels,
            audio_format::channel_ordering::interleaved
        };
    }
    if (encoding_name == "L24") {
        return audio_format {
            audio_format::byte_order::be, audio_encoding::pcm_s24, clock_rate, num_channels,
            audio_format::channel_ordering::interleaved
        };
    }
    if (encoding_name == "L32") {
        return audio_format {
            audio_format::byte_order::be, audio_encoding::pcm_s32, clock_rate, num_channels,
            audio_format::channel_ordering::interleaved
        };
    }
    return std::nullopt;
}

std::optional<rav::sdp::format> rav::sdp::format::from_audio_format(const audio_format& input_format) {
    format output_format;

    switch (input_format.encoding) {
        case audio_encoding::undefined:
        case audio_encoding::pcm_s8:
        case audio_encoding::pcm_s32:
        case audio_encoding::pcm_f32:
        case audio_encoding::pcm_f64:
            return std::nullopt;
        case audio_encoding::pcm_u8:
            output_format.encoding_name = "L8";  // https://datatracker.ietf.org/doc/html/rfc3551#section-4.5.10
            break;
        case audio_encoding::pcm_s16:
            output_format.encoding_name = "L16";  // https://datatracker.ietf.org/doc/html/rfc3551#section-4.5.11
            break;
        case audio_encoding::pcm_s24:
            output_format.encoding_name = "L24";  // https://datatracker.ietf.org/doc/html/rfc3190#section-4
            break;
    }

    output_format.clock_rate = input_format.sample_rate;
    output_format.num_channels = input_format.num_channels;
    output_format.payload_type = 98;

    return output_format;
}

rav::sdp::format::parse_result<rav::sdp::format> rav::sdp::format::parse_new(const std::string_view line) {
    string_parser parser(line);

    format map;

    if (const auto payload_type = parser.read_int<uint8_t>()) {
        map.payload_type = *payload_type;
        if (!parser.skip(' ')) {
            return parse_result<format>::err("rtpmap: expecting space after payload type");
        }
    } else {
        return parse_result<format>::err("rtpmap: invalid payload type");
    }

    if (const auto encoding_name = parser.split('/')) {
        map.encoding_name = *encoding_name;
    } else {
        return parse_result<format>::err("rtpmap: failed to parse encoding name");
    }

    if (const auto clock_rate = parser.read_int<uint32_t>()) {
        map.clock_rate = *clock_rate;
    } else {
        return parse_result<format>::err("rtpmap: invalid clock rate");
    }

    if (parser.skip('/')) {
        if (const auto num_channels = parser.read_int<uint32_t>()) {
            // Note: strictly speaking the encoding parameters can be anything, but as of now it's only used for
            // channels.
            map.num_channels = *num_channels;
        } else {
            return parse_result<format>::err("rtpmap: failed to parse number of channels");
        }
    } else {
        map.num_channels = 1;
    }

    return parse_result<format>::ok(map);
}
