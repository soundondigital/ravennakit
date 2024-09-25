/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/result.hpp"

#include <catch2/catch_all.hpp>

#include "ravennakit/core/util.hpp"

TEST_CASE("result", "[result]") {
    SECTION("Test ok") {
        const auto result = rav::result<int, int>::ok(42);
        REQUIRE(result.is_ok());
        REQUIRE_FALSE(result.is_err());
    }

    SECTION("Test err") {
        const auto result = rav::result<int, int>::err(-42);
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.is_err());
    }

    SECTION("Test err const char*") {
        const auto result = rav::result<int, const char*>::err("This is an error");
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.is_err());
        REQUIRE(std::strcmp(result.get_err(), "This is an error") == 0);
    }

    SECTION("Test getting value") {
        constexpr auto test_string =
            "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.";
        auto result = rav::result<std::string, std::string>::ok(test_string);
        REQUIRE(result.is_ok());
        REQUIRE(result.get_ok() == test_string);
        REQUIRE_THROWS(result.get_err());
        REQUIRE(result.move_ok() == test_string);
        REQUIRE(result.move_ok().empty());
    }

    SECTION("Test ok void result") {
        auto result = rav::result<void, std::string>::ok();
        REQUIRE(result.is_ok());
    }

    SECTION("Test err void result") {
        auto result = rav::result<void, std::string>::err("BlahBlahBlah");
        REQUIRE_FALSE(result.is_ok());
        REQUIRE(result.is_err());
        REQUIRE(result.get_err() == "BlahBlahBlah");
    }
}
