/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_group.hpp"

#include "ravennakit/core/string_parser.hpp"
#include "ravennakit/core/support.hpp"

#include <fmt/ranges.h>

tl::expected<rav::sdp::Group, std::string> rav::sdp::parse_group(const std::string_view line) {
    StringParser parser(line);

    const auto type = parser.read_until(' ');
    if (!type) {
        return tl::unexpected("Invalid group type");
    }

    if (*type != "DUP") {
        return tl::unexpected(fmt::format("Unsupported group type ({})"));
    }

    Group group;
    group.type = Group::Type::dup;

    for (auto tag = parser.split(' '); tag.has_value(); tag = parser.split(' ')) {
        group.tags.emplace_back(*tag);
    }

    return group;
}

std::string rav::sdp::to_string(const Group& input) {
    return fmt::format("a=group:DUP {}", fmt::join(input.tags, " "));
}
