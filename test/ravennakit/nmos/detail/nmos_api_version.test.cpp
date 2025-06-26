/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/nmos/detail/nmos_api_version.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::nmos::ApiVersion") {
    rav::nmos::ApiVersion version;

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

    SECTION("To string") {
        version = {0, 0};
        CHECK_FALSE(version.is_valid());
    }

    SECTION("To string") {
        version = {1, 0};
        CHECK(version.to_string() == "v1.0");
    }

    SECTION("To string with invalid version") {
        version = {0, 0};
        CHECK(version.to_string() == "v0.0");
    }

    SECTION("To string with negative version") {
        version = {-1, -1};
        CHECK(version.to_string() == "v-1.-1");
    }

    SECTION("To string with large version") {
        version = {1000, 2000};
        CHECK(version.to_string() == "v1000.2000");
    }

    SECTION("From v1.2") {
        auto v = rav::nmos::ApiVersion::from_string("v1.2");
        REQUIRE(v.has_value());
        CHECK(v->major == 1);
        CHECK(v->minor == 2);
    }

    SECTION("From v1.2 with leading spaces") {
        auto v = rav::nmos::ApiVersion::from_string(" v1.2");
        REQUIRE_FALSE(v.has_value());
    }

    SECTION("From v1.2 with trailing spaces") {
        auto v = rav::nmos::ApiVersion::from_string("v1.2 ");
        REQUIRE_FALSE(v.has_value());
    }

    SECTION("From incomplete") {
        auto v = rav::nmos::ApiVersion::from_string("v1.");
        CHECK_FALSE(v.has_value());

        v = rav::nmos::ApiVersion::from_string("v12");
        CHECK_FALSE(v.has_value());

        v = rav::nmos::ApiVersion::from_string("v.2");
        CHECK_FALSE(v.has_value());
    }
}
