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

#include "ravennakit/core/expected.hpp"

#include <string>

namespace rav::sdp {

/**
 * Represents a group of media descriptions in an SDP message.
 * RFC 5888: https://datatracker.ietf.org/doc/html/rfc5888
 * RFC 7104: https://datatracker.ietf.org/doc/html/rfc7104
 */
class Group {
  public:
    enum class Type {
        undefined = 0,
        dup,
    };

    Type type = Type::undefined;
    std::vector<std::string> tags;
};

/**
 * Parses a group line from an SDP message.
 * @param line The line to parse.
 * @return A tl::expected object containing the parsed group or an error message.
 */
[[nodiscard]] tl::expected<Group, std::string> parse_group(std::string_view line);

/**
 * @return The group encoded as string.
 */
[[nodiscard]] std::string to_string(const Group& input);

}  // namespace rav::sdp
