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

#include <catch2/catch_all.hpp>

TEST_CASE("rav::sdp::Group") {
    SECTION("Parse group line") {
        const auto group_line = "DUP primary secondary";
        auto group = rav::sdp::parse_group(group_line);
        REQUIRE(group);
        REQUIRE(group->type == rav::sdp::Group::Type::dup);
        REQUIRE(group->tags.size() == 2);
        REQUIRE(group->tags[0] == "primary");
        REQUIRE(group->tags[1] == "secondary");
    }

    SECTION("Parse group of three") {
        const auto group_line = "DUP primary secondary tertiary";
        auto group = rav::sdp::parse_group(group_line);
        REQUIRE(group);
        REQUIRE(group->type == rav::sdp::Group::Type::dup);
        auto tags = group->tags;
        REQUIRE(tags.size() == 3);
        REQUIRE(tags[0] == "primary");
        REQUIRE(tags[1] == "secondary");
        REQUIRE(tags[2] == "tertiary");
    }

    SECTION("To string") {
        rav::sdp::Group group;
        group.type = rav::sdp::Group::Type::dup;
        group.tags.push_back("primary");
        group.tags.push_back("secondary");
        REQUIRE(rav::sdp::to_string(group) == "a=group:DUP primary secondary");
    }
}
