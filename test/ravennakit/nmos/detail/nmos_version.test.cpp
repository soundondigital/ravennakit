/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/nmos/detail/nmos_timestamp.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::nmos::Version") {
    rav::nmos::Version version;

    SECTION("Default constructor") {
        CHECK_FALSE(version.is_valid());
    }

    SECTION("Valid version") {
        version = {1, 0};
        CHECK(version.is_valid());
    }

    SECTION("Invalid version") {
        version = {0, 0};
        CHECK_FALSE(version.is_valid());
    }

    SECTION("to_string") {
        version = {1439299836, 10};
        CHECK(version.to_string() == "1439299836:10");

        version = {0, 123456789};
        CHECK(version.to_string() == "0:123456789");
    }

    SECTION("From string") {
        auto v = rav::nmos::Version::from_string("1439299836:10");
        REQUIRE(v.has_value());
        CHECK(v->seconds == 1439299836);
        CHECK(v->nanoseconds == 10);

        // Leading whitespace
        v = rav::nmos::Version::from_string(" 1439299836:10");
        REQUIRE_FALSE(v.has_value());

        // Trailing whitespace
        v = rav::nmos::Version::from_string("1439299836:10 ");
        REQUIRE_FALSE(v.has_value());
    }
}
