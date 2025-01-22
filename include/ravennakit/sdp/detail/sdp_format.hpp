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

#include "ravennakit/core/result.hpp"
#include "ravennakit/core/audio/audio_format.hpp"

#include <string>

namespace rav::sdp {

/**
 * Holds the information of an RTP map.
 */
struct format {
    uint8_t payload_type {};
    std::string encoding_name;
    uint32_t clock_rate {};
    uint32_t num_channels {};

    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, std::string>;

    [[nodiscard]] std::string to_string() const;

    /**
     * @return The format as audio_format, or nullopt if the format is not supported or cannot be converted.
     */
    [[nodiscard]] std::optional<audio_format> to_audio_format() const;

    /**
     * @return The format as audio_format, or nullopt if the format is not supported or cannot be converted.
     */
    [[nodiscard]] static std::optional<format> from_audio_format(const audio_format& input_format) ;

    /**
     * Parses a format from a string.
     * @param line The string to parse.
     * @return A result indicating success or failure. When parsing fails, the error message will contain a
     * description of what went wrong.
     */
    static parse_result<format> parse_new(std::string_view line);
};

bool operator==(const format& lhs, const format& rhs);
bool operator!=(const format& lhs, const format& rhs);

inline auto format_as(const format& f) {
    return f.to_string();
}

}
