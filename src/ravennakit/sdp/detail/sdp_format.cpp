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

tl::expected<rav::sdp::Format, std::string> rav::sdp::parse_format(const std::string_view line) {
    StringParser parser(line);

    Format map;

    if (const auto payload_type = parser.read_int<uint8_t>()) {
        map.payload_type = *payload_type;
        if (!parser.skip(' ')) {
            return tl::unexpected("rtpmap: expecting space after payload type");
        }
    } else {
        return tl::unexpected("rtpmap: invalid payload type");
    }

    if (const auto encoding_name = parser.split('/')) {
        map.encoding_name = *encoding_name;
    } else {
        return tl::unexpected("rtpmap: failed to parse encoding name");
    }

    if (const auto clock_rate = parser.read_int<uint32_t>()) {
        map.clock_rate = *clock_rate;
    } else {
        return tl::unexpected("rtpmap: invalid clock rate");
    }

    if (parser.skip('/')) {
        if (const auto num_channels = parser.read_int<uint32_t>()) {
            // Note: strictly speaking the encoding parameters can be anything, but as of now it's only used for
            // channels.
            map.num_channels = *num_channels;
        } else {
            return tl::unexpected("rtpmap: failed to parse number of channels");
        }
    } else {
        map.num_channels = 1;
    }

    return map;
}

std::optional<rav::sdp::Format> rav::sdp::make_audio_format(const AudioFormat& input_format) {
    Format output_format;

    switch (input_format.encoding) {
        case AudioEncoding::undefined:
        case AudioEncoding::pcm_s8:
        case AudioEncoding::pcm_s32:
        case AudioEncoding::pcm_f32:
        case AudioEncoding::pcm_f64:
            return std::nullopt;
        case AudioEncoding::pcm_u8:
            output_format.encoding_name = "L8";  // https://datatracker.ietf.org/doc/html/rfc3551#section-4.5.10
            break;
        case AudioEncoding::pcm_s16:
            output_format.encoding_name = "L16";  // https://datatracker.ietf.org/doc/html/rfc3551#section-4.5.11
            break;
        case AudioEncoding::pcm_s24:
            output_format.encoding_name = "L24";  // https://datatracker.ietf.org/doc/html/rfc3190#section-4
            break;
    }

    output_format.clock_rate = input_format.sample_rate;
    output_format.num_channels = input_format.num_channels;
    output_format.payload_type = 98;

    return output_format;
}

std::optional<rav::AudioFormat> rav::sdp::make_audio_format(const Format& input_format) {
    if (input_format.encoding_name == "L16") {
        return AudioFormat {
            AudioFormat::ByteOrder::be, AudioEncoding::pcm_s16, AudioFormat::ChannelOrdering::interleaved,
            input_format.clock_rate, input_format.num_channels
        };
    }
    if (input_format.encoding_name == "L24") {
        return AudioFormat {
            AudioFormat::ByteOrder::be, AudioEncoding::pcm_s24, AudioFormat::ChannelOrdering::interleaved,
            input_format.clock_rate, input_format.num_channels
        };
    }
    if (input_format.encoding_name == "L32") {
        return AudioFormat {
            AudioFormat::ByteOrder::be, AudioEncoding::pcm_s32, AudioFormat::ChannelOrdering::interleaved,
            input_format.clock_rate, input_format.num_channels
        };
    }
    return std::nullopt;
}

std::string rav::sdp::to_string(const Format& input_format) {
    return fmt::format(
        "{} {}/{}/{}", input_format.payload_type, input_format.encoding_name, input_format.clock_rate,
        input_format.num_channels
    );
}
