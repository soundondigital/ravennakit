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

#include <cstdint>
#include <string_view>
#include <tl/expected.hpp>

namespace rav::sdp {

/**
 * A type representing the time field (t=*) of an SDP session description.
 * Defined as seconds since January 1, 1900, UTC.
 */
struct TimeActiveField {
    /// The start time of the session.
    int64_t start_time {0};
    /// The stop time of the session.
    int64_t stop_time {0};

    /**
     * Validates the values of this structure.
     * @return A result indicating success or failure. When validation fails, the error message will contain a
     * description.
     */
    [[nodiscard]] tl::expected<void, std::string> validate() const;

    /**
     * Converts the time field to a string.
     * @return The time field as a string, or an error message if the conversion fails.
     */
    [[nodiscard]] tl::expected<std::string, std::string> to_string() const;

    /// A type alias for a parse result.
    template<class T>
    using ParseResult = result<T, std::string>;

    /**
     * Parses a time field from a string.
     * @param line The string to parse.
     * @return A pair containing the parse result and the time field.
     */
    static ParseResult<TimeActiveField> parse_new(std::string_view line);
};

}  // namespace rav::sdp
