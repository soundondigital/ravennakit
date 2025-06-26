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

#include "ravennakit/core/audio/audio_format.hpp"

#include <string>

namespace rav::sdp {

/**
 * Holds the information of an RTP map.
 */
struct Format {
    uint8_t payload_type {};
    std::string encoding_name;
    uint32_t clock_rate {};
    uint32_t num_channels {};
};

/**
 * Parses a format from a string.
 * @param line The string to parse.
 * @return A result indicating success or failure. When parsing fails, the error message will contain a
 * description of what went wrong.
 */
[[nodiscard]] tl::expected<Format, std::string> parse_format(std::string_view line);

/**
 * @return The format as audio_format, or nullopt if the format is not supported or cannot be converted.
 */
[[nodiscard]] std::optional<Format> make_audio_format(const AudioFormat& input_format);

/**
 * @return The format as audio_format, or nullopt if the format is not supported or cannot be converted.
 */
[[nodiscard]] std::optional<AudioFormat> make_audio_format(const Format& input_format);

/**
 * Converts a Format to an SDP compatible string.
 * @param input_format The input format.
 * @return The encoded string.
 */
[[nodiscard]] std::string to_string(const Format& input_format);

}  // namespace rav::sdp
