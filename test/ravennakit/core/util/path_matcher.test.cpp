/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/util/path_matcher.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("PathMatcher") {
    REQUIRE(rav::PathMatcher::match("/test", "/test"));
    REQUIRE_FALSE(rav::PathMatcher::match("/", "/test"));
    REQUIRE(rav::PathMatcher::match("/user/5", "**"));
    REQUIRE(rav::PathMatcher::match("user/5", "**"));
}
