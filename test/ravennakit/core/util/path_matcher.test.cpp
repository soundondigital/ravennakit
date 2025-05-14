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
    REQUIRE(rav::PathMatcher::match("/", "/").value());
    REQUIRE(rav::PathMatcher::match("/test", "/test").value());
    REQUIRE_FALSE(rav::PathMatcher::match("/test", "/tes").value());
    REQUIRE(rav::PathMatcher::match("/test/", "/test").value());
    REQUIRE_FALSE(rav::PathMatcher::match("/test/", "/tes").value());
    REQUIRE(rav::PathMatcher::match("/test", "/test/").value());
    REQUIRE_FALSE(rav::PathMatcher::match("/", "/test").value());
    REQUIRE_FALSE(rav::PathMatcher::match("/non-existent", "/").value());
    REQUIRE(rav::PathMatcher::match("/", "**").value());
    REQUIRE(rav::PathMatcher::match("/user/5", "**").value());
    REQUIRE(rav::PathMatcher::match("/user/5", "/**").value());
    REQUIRE(rav::PathMatcher::match("/user/5", "/user/**").value());
    REQUIRE_FALSE(rav::PathMatcher::match("/user2/5", "/user/**").value());
    REQUIRE(rav::PathMatcher::match("/user/5/something", "/user/**").value());
    REQUIRE_FALSE(rav::PathMatcher::match("/user2/5/something", "/user/**").value());
    REQUIRE(rav::PathMatcher::match("/user/5/something/else", "/user/**").value());
    REQUIRE_FALSE(rav::PathMatcher::match("/user2/5/something/else", "/user/**").value());
    REQUIRE(
        rav::PathMatcher::match("/user/5/something/else/end", "/user/**/end")
        == rav::PathMatcher::Error::invalid_recursive_wildcard
    );
    REQUIRE(
        rav::PathMatcher::match("/one/two/three/four/five/six/seven", "/one/**/four/**/seven")
        == rav::PathMatcher::Error::invalid_recursive_wildcard
    );
    REQUIRE(
        rav::PathMatcher::match("/one/two/three/four/five/six/eight", "/one/**/four/**/seven")
        == rav::PathMatcher::Error::invalid_recursive_wildcard
    );
    REQUIRE(rav::PathMatcher::match("user/5", "**"));

    // If path or pattern are empty, return false
    REQUIRE_FALSE(rav::PathMatcher::match("", "/").value());
    REQUIRE_FALSE(rav::PathMatcher::match("/", "").value());
    REQUIRE_FALSE(rav::PathMatcher::match("", "").value());

    {
        rav::PathMatcher::Parameters parameters;
        REQUIRE(rav::PathMatcher::match("/user/1", "/user/{id}", &parameters).value());
        REQUIRE(parameters.get("id") != nullptr);
        REQUIRE(*parameters.get("id") == "1");
        REQUIRE(*parameters.get_as<int>("id") == 1);
    }

    REQUIRE_FALSE(rav::PathMatcher::match("/user/", "/user/{id}").value());
    REQUIRE_FALSE(rav::PathMatcher::match("/user", "/user/{id}").value());

    {
        rav::PathMatcher::Parameters parameters;
        REQUIRE(rav::PathMatcher::match("/user/123", "/user/{id}", &parameters).value());
        REQUIRE(parameters.get("id") != nullptr);
        REQUIRE(*parameters.get("id") == "123");
        REQUIRE(*parameters.get_as<int>("id") == 123);
    }

    REQUIRE(rav::PathMatcher::match("/user/123", "/user/{id}") == rav::PathMatcher::Error::invalid_argument);

    {
        rav::PathMatcher::Parameters parameters;
        REQUIRE(rav::PathMatcher::match("/user/abc123", "/user/abc{id}", &parameters).value());
        REQUIRE(parameters.get("id") != nullptr);
        REQUIRE(*parameters.get("id") == "123");
        REQUIRE(*parameters.get_as<int>("id") == 123);
        parameters.clear();
        REQUIRE(rav::PathMatcher::match("/user/abc123", "/user/ab{id}", &parameters).value());
        REQUIRE(parameters.get("id") != nullptr);
        REQUIRE(*parameters.get("id") == "c123");
        REQUIRE_FALSE(parameters.get_as<int>("id"));
    }

    {
        rav::PathMatcher::Parameters parameters;
        REQUIRE(rav::PathMatcher::match("/user/123def", "/user/{id}def", &parameters).value());
        REQUIRE(parameters.get("id") != nullptr);
        REQUIRE(*parameters.get("id") == "123");
        REQUIRE(*parameters.get_as<int>("id") == 123);
        parameters.clear();
        REQUIRE(rav::PathMatcher::match("/user/123def", "/user/{id}ef", &parameters).value());
        REQUIRE(parameters.get("id") != nullptr);
        REQUIRE(*parameters.get("id") == "123d");
        REQUIRE(parameters.get_as<int>("id") == 123);
    }

    {
        rav::PathMatcher::Parameters parameters;
        REQUIRE(rav::PathMatcher::match("/user/abc123def", "/user/abc{id}def", &parameters).value());
        REQUIRE(parameters.get("id") != nullptr);
        REQUIRE(*parameters.get("id") == "123");
        REQUIRE(*parameters.get_as<int>("id") == 123);
    }

    REQUIRE_FALSE(rav::PathMatcher::match("/user/ab123def", "/user/abc{id}def").value());
    REQUIRE_FALSE(rav::PathMatcher::match("/user/ab123ef", "/user/abc{id}def").value());

    {
        rav::PathMatcher::Parameters parameters;
        REQUIRE(rav::PathMatcher::match("/user/5/item/6", "/user/{id}/item/{item}", &parameters).value());
        REQUIRE(parameters.get("id") != nullptr);
        REQUIRE(*parameters.get("id") == "5");
        REQUIRE(*parameters.get_as<int>("id") == 5);
        REQUIRE(parameters.get("item") != nullptr);
        REQUIRE(*parameters.get("item") == "6");
        REQUIRE(*parameters.get_as<int>("item") == 6);
        REQUIRE(parameters.get("nonexistent") == nullptr);
    }

    {
        rav::PathMatcher::Parameters parameters;
        REQUIRE(rav::PathMatcher::match("/user/john", "/user/{name}", &parameters).value());
        REQUIRE(parameters.get("name") != nullptr);
        REQUIRE(*parameters.get("name") == "john");
        REQUIRE(parameters.get_as<int>("name") == std::nullopt);
        REQUIRE(parameters.get("id") == nullptr);
    }
}
