/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_connection_info.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("media_description | connection_info_field") {
    SECTION("Parse connection line") {
        auto result = rav::sdp::ConnectionInfoField::parse_new("c=IN IP4 239.1.15.52");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::sdp::NetwType::internet);
        REQUIRE(connection.address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(connection.address == "239.1.15.52");
        REQUIRE(connection.ttl.has_value() == false);
        REQUIRE(connection.number_of_addresses.has_value() == false);
    }

    SECTION("Parse connection line with ttl") {
        auto result = rav::sdp::ConnectionInfoField::parse_new("c=IN IP4 239.1.15.52/15");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::sdp::NetwType::internet);
        REQUIRE(connection.address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(connection.address == "239.1.15.52");
        REQUIRE(connection.ttl.has_value());
        REQUIRE(*connection.ttl == 15);
        REQUIRE(connection.number_of_addresses.has_value() == false);
    }

    SECTION("Parse connection line with ttl and number of addresses") {
        auto result = rav::sdp::ConnectionInfoField::parse_new("c=IN IP4 239.1.15.52/15/3");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::sdp::NetwType::internet);
        REQUIRE(connection.address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(connection.address == "239.1.15.52");
        REQUIRE(connection.ttl.has_value());
        REQUIRE(*connection.ttl == 15);
        REQUIRE(connection.number_of_addresses.has_value());
        REQUIRE(*connection.number_of_addresses == 3);
    }

    SECTION("Parse ipv6 connection line with number of addresses") {
        auto result = rav::sdp::ConnectionInfoField::parse_new("c=IN IP6 ff00::db8:0:101/3");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::sdp::NetwType::internet);
        REQUIRE(connection.address_type == rav::sdp::AddrType::ipv6);
        REQUIRE(connection.address == "ff00::db8:0:101");
        REQUIRE(connection.ttl.has_value() == false);
        REQUIRE(connection.number_of_addresses.has_value());
        REQUIRE(*connection.number_of_addresses == 3);
    }

    SECTION("Parse ipv6 connection line with ttl and number of addresses (which should fail)") {
        auto result = rav::sdp::ConnectionInfoField::parse_new("c=IN IP6 ff00::db8:0:101/127/3");
        REQUIRE(result.is_err());
    }

    SECTION("To string") {
        rav::sdp::ConnectionInfoField connection;
        REQUIRE(connection.to_string().error() == "connection: network type is undefined");
        connection.network_type = rav::sdp::NetwType::internet;
        REQUIRE(connection.to_string().error() == "connection: address type is undefined");
        connection.address_type = rav::sdp::AddrType::ipv4;
        REQUIRE(connection.to_string().error() == "connection: address is empty");
        connection.address = "239.1.16.51";
        REQUIRE(connection.to_string().error() == "connection: ttl is required for ipv4 address");
        connection.ttl = 15;
        REQUIRE(connection.to_string().value() == "c=IN IP4 239.1.16.51/15");
    }
}
