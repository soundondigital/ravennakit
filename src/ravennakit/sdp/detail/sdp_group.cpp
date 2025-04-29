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

rav::sdp::Group::Type rav::sdp::Group::get_type() const {
    return type_;
}

void rav::sdp::Group::set_type(const Type type) {
    type_ = type;
}

const std::vector<std::string>& rav::sdp::Group::get_tags() const {
    return tags_;
}

const std::string& rav::sdp::Group::get_tag(const size_t index) const {
    if (index < tags_.size()) {
        return tags_[index];
    }
    return rav::get_global_const_instance_of_type<std::string>();
}

void rav::sdp::Group::add_tag(std::string tag) {
    tags_.emplace_back(std::move(tag));
}

tl::expected<std::string, std::string> rav::sdp::Group::to_string() const {
    return fmt::format("a=group:DUP {}", fmt::join(tags_, " "));
}

bool rav::sdp::Group::empty() const {
    return tags_.empty();
}

tl::expected<rav::sdp::Group, std::string> rav::sdp::Group::parse_new(const std::string_view line) {
    StringParser parser(line);

    const auto type = parser.read_until(' ');
    if (!type) {
        return tl::unexpected("Invalid group type");
    }

    if (*type != "DUP") {
        return tl::unexpected(fmt::format("Unsupported group type ({})"));
    }

    Group group;
    group.type_ = Type::dup;

    for (auto tag = parser.split(' '); tag.has_value(); tag = parser.split(' ')) {
        group.tags_.emplace_back(*tag);
    }

    return group;
}
