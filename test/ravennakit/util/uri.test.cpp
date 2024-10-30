/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/util/uri.hpp"

#include <catch2/catch_all.hpp>

// https://datatracker.ietf.org/doc/html/rfc3986/

TEST_CASE("uri parse") {
    SECTION("Full URI") {
        auto uri = rav::uri::parse(
            "foo://user:pass@example.com:8042/some/path%20with%20space?key=value+space&key2=value2#fragment"
        );
        REQUIRE(uri.scheme == "foo");
        REQUIRE(uri.user == "user");
        REQUIRE(uri.password == "pass");
        REQUIRE(uri.host == "example.com");
        REQUIRE(uri.port == 8042);
        REQUIRE(uri.path == "/some/path with space");
        REQUIRE(uri.query == std::map<std::string, std::string> {{"key", "value space"}, {"key2", "value2"}});
        REQUIRE(uri.fragment == "fragment");
    }

    SECTION("Minimal URI") {
        auto uri = rav::uri::parse("foo://");
        REQUIRE(uri.scheme == "foo");
        REQUIRE(uri.user.empty());
        REQUIRE(uri.password.empty());
        REQUIRE(uri.host.empty());
        REQUIRE_FALSE(uri.port.has_value());
        REQUIRE(uri.path.empty());
        REQUIRE(uri.query.empty());
        REQUIRE(uri.fragment.empty());
    }

    SECTION("Only host") {
        auto uri = rav::uri::parse("foo://example.com");
        REQUIRE(uri.scheme == "foo");
        REQUIRE(uri.user.empty());
        REQUIRE(uri.password.empty());
        REQUIRE(uri.host == "example.com");
        REQUIRE_FALSE(uri.port.has_value());
        REQUIRE(uri.path.empty());
        REQUIRE(uri.query.empty());
        REQUIRE(uri.fragment.empty());
    }

    SECTION("With port") {
        auto uri = rav::uri::parse("foo://example.com:1234");
        REQUIRE(uri.scheme == "foo");
        REQUIRE(uri.user.empty());
        REQUIRE(uri.password.empty());
        REQUIRE(uri.host == "example.com");
        REQUIRE(uri.port == 1234);
        REQUIRE(uri.path.empty());
        REQUIRE(uri.query.empty());
        REQUIRE(uri.fragment.empty());
    }

    SECTION("With path") {
        auto uri = rav::uri::parse("foo://example.com:1234/some/path");
        REQUIRE(uri.scheme == "foo");
        REQUIRE(uri.user.empty());
        REQUIRE(uri.password.empty());
        REQUIRE(uri.host == "example.com");
        REQUIRE(uri.port == 1234);
        REQUIRE(uri.path == "/some/path");
        REQUIRE(uri.query.empty());
        REQUIRE(uri.fragment.empty());
    }

    SECTION("With query") {
        auto uri = rav::uri::parse("foo://example.com:1234/some/path?key1=value1&key2=value2");
        REQUIRE(uri.scheme == "foo");
        REQUIRE(uri.user.empty());
        REQUIRE(uri.password.empty());
        REQUIRE(uri.host == "example.com");
        REQUIRE(uri.port == 1234);
        REQUIRE(uri.path == "/some/path");
        REQUIRE(uri.query == std::map<std::string, std::string> {{"key1", "value1"}, {"key2", "value2"}});
        REQUIRE(uri.fragment.empty());
    }

    SECTION("With fragment") {
        auto uri = rav::uri::parse("foo://example.com:1234/some/path#fragment");
        REQUIRE(uri.scheme == "foo");
        REQUIRE(uri.user.empty());
        REQUIRE(uri.password.empty());
        REQUIRE(uri.host == "example.com");
        REQUIRE(uri.port == 1234);
        REQUIRE(uri.path == "/some/path");
        REQUIRE(uri.query.empty());
        REQUIRE(uri.fragment == "fragment");
    }
}

TEST_CASE("uri to_string") {
    const rav::uri uri {
        "foo",
        "user",
        "pass",
        "example.com",
        8042,
        "/some/path with space",
        {{"key1", "value with space"}, {"key2", "value2"}},
        "fragment"
    };
    auto result = uri.to_string();
    REQUIRE(
        result == "foo://user:pass@example.com:8042/some/path%20with%20space?key1=value+with+space&key2=value2#fragment"
    );
}

TEST_CASE("uri decode") {
    auto result = rav::uri::decode("foo%20bar%21+");
    REQUIRE(result == "foo bar!+");

    result = rav::uri::decode("foo%20bar%21+", true);
    REQUIRE(result == "foo bar! ");

    result = rav::uri::decode(
        "%20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2D%2E%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%5F%60%7B%7C%7D%7E"
    );
    REQUIRE(result == " !\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~");  // Includes all unreserved characters
}

TEST_CASE("uri encode") {
    auto result = rav::uri::encode(" !\"#$%&'()*+,/:;<=>?@[\\]^`{|}", false, true);
    REQUIRE(result == "%20%21%22%23%24%25%26%27%28%29%2A%2B%2C%2F%3A%3B%3C%3D%3E%3F%40%5B%5C%5D%5E%60%7B%7C%7D");

    // Unreserved characters
    result = rav::uri::encode("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~");
    REQUIRE(result == "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~");

    result = rav::uri::encode(" ", true);
    REQUIRE(result == "+");

    result = rav::uri::encode(" ", false);
    REQUIRE(result == "%20");

    result = rav::uri::encode("/", true, true);
    REQUIRE(result == "%2F");

    result = rav::uri::encode("/", true, false);
    REQUIRE(result == "/");
}
