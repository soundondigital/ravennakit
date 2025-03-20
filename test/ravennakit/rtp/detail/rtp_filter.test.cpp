/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/detail/rtp_filter.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rtp_filter") {
    const auto connection_address = asio::ip::make_address("239.3.8.1");
    rav::rtp::Filter filter(connection_address);
    SECTION("matches") {
        REQUIRE(filter.connection_address() == connection_address);
    }

    SECTION("matches") {
        REQUIRE(filter.empty());
        filter.add_filter(asio::ip::make_address("192.168.1.2"), rav::sdp::filter_mode::exclude);
        REQUIRE_FALSE(filter.empty());
    }

    SECTION("is_valid_source with no filters") {
        auto matches = filter.is_valid_source(connection_address, asio::ip::make_address("127.0.0.1"));
        REQUIRE(matches);
    }

    SECTION("is_valid_source with invalid connection address") {
        auto matches =
            filter.is_valid_source(asio::ip::make_address("192.168.1.2"), asio::ip::make_address("127.0.0.1"));
        REQUIRE_FALSE(matches);
    }

    SECTION("is_valid_source with single exclude address") {
        filter.add_filter(asio::ip::make_address("192.168.1.2"), rav::sdp::filter_mode::exclude);
        auto matches = filter.is_valid_source(connection_address, asio::ip::make_address("127.0.0.1"));
        REQUIRE(matches);
        matches = filter.is_valid_source(connection_address, asio::ip::make_address("192.168.1.2"));
        REQUIRE_FALSE(matches);
    }

    SECTION("is_valid_source with single include address") {
        filter.add_filter(asio::ip::make_address("192.168.1.2"), rav::sdp::filter_mode::include);
        auto matches = filter.is_valid_source(connection_address, asio::ip::make_address("127.0.0.1"));
        REQUIRE_FALSE(matches);
        matches = filter.is_valid_source(connection_address, asio::ip::make_address("192.168.1.2"));
        REQUIRE(matches);
    }

    SECTION("add_filter with single include address") {
        auto src_filter = rav::sdp::source_filter::parse_new(" incl IN IP4 239.3.8.1 192.168.16.52");
        REQUIRE(src_filter.is_ok());
        REQUIRE(filter.add_filter(src_filter.get_ok()) == 1);
        REQUIRE_FALSE(filter.empty());
        REQUIRE(filter.connection_address() == asio::ip::make_address("239.3.8.1"));
        REQUIRE(filter.is_valid_source(asio::ip::make_address("239.3.8.1"), asio::ip::make_address("192.168.16.52")));
        REQUIRE_FALSE(
            filter.is_valid_source(asio::ip::make_address("239.3.8.1"), asio::ip::make_address("192.168.16.53"))
        );
    }

    SECTION("add_filter with single exclude address") {
        auto src_filter = rav::sdp::source_filter::parse_new(" excl IN IP4 239.3.8.1 192.168.16.52");
        REQUIRE(src_filter.is_ok());
        REQUIRE(filter.add_filter(src_filter.get_ok()) == 1);
        REQUIRE_FALSE(filter.empty());
        REQUIRE(filter.connection_address() == asio::ip::make_address("239.3.8.1"));
        REQUIRE_FALSE(
            filter.is_valid_source(asio::ip::make_address("239.3.8.1"), asio::ip::make_address("192.168.16.52"))
        );
        REQUIRE(filter.is_valid_source(asio::ip::make_address("239.3.8.1"), asio::ip::make_address("192.168.16.53")));
    }
}
