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

TEST_CASE("rav::sdp::ConnectionInfoField") {
    SECTION("Parse connection line") {
        auto connection = rav::sdp::parse_connection_info("c=IN IP4 239.1.15.52");
        REQUIRE(connection);
        REQUIRE(connection->network_type == rav::sdp::NetwType::internet);
        REQUIRE(connection->address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(connection->address == "239.1.15.52");
        REQUIRE(connection->ttl.has_value() == false);
        REQUIRE(connection->number_of_addresses.has_value() == false);
    }

    SECTION("Parse connection line with ttl") {
        auto connection = rav::sdp::parse_connection_info("c=IN IP4 239.1.15.52/15");
        REQUIRE(connection);
        REQUIRE(connection->network_type == rav::sdp::NetwType::internet);
        REQUIRE(connection->address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(connection->address == "239.1.15.52");
        REQUIRE(connection->ttl.has_value());
        REQUIRE(*connection->ttl == 15);
        REQUIRE(connection->number_of_addresses.has_value() == false);
    }

    SECTION("Parse connection line with ttl and number of addresses") {
        auto connection = rav::sdp::parse_connection_info("c=IN IP4 239.1.15.52/15/3");
        REQUIRE(connection);
        REQUIRE(connection->network_type == rav::sdp::NetwType::internet);
        REQUIRE(connection->address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(connection->address == "239.1.15.52");
        REQUIRE(connection->ttl.has_value());
        REQUIRE(*connection->ttl == 15);
        REQUIRE(connection->number_of_addresses.has_value());
        REQUIRE(*connection->number_of_addresses == 3);
    }

    SECTION("Parse ipv6 connection line with number of addresses") {
        auto connection = rav::sdp::parse_connection_info("c=IN IP6 ff00::db8:0:101/3");
        REQUIRE(connection);
        REQUIRE(connection->network_type == rav::sdp::NetwType::internet);
        REQUIRE(connection->address_type == rav::sdp::AddrType::ipv6);
        REQUIRE(connection->address == "ff00::db8:0:101");
        REQUIRE(connection->ttl.has_value() == false);
        REQUIRE(connection->number_of_addresses.has_value());
        REQUIRE(*connection->number_of_addresses == 3);
    }

    SECTION("Parse ipv6 connection line with ttl and number of addresses (which should fail)") {
        auto result = rav::sdp::parse_connection_info("c=IN IP6 ff00::db8:0:101/127/3");
        REQUIRE(!result);
    }

    SECTION("Validate") {
        rav::sdp::ConnectionInfoField connection;
        REQUIRE(rav::sdp::validate(connection).error() == "connection: network type is undefined");
        connection.network_type = rav::sdp::NetwType::internet;
        REQUIRE(rav::sdp::validate(connection).error() == "connection: address type is undefined");
        connection.address_type = rav::sdp::AddrType::ipv4;
        REQUIRE(rav::sdp::validate(connection).error() == "connection: address is empty");
        connection.address = "239.1.16.51";
        REQUIRE(rav::sdp::validate(connection).error() == "connection: ttl is required for ipv4 address");
        connection.ttl = 15;
        REQUIRE(rav::sdp::validate(connection).has_value());
    }

    SECTION("To string") {
        rav::sdp::ConnectionInfoField connection;
        connection.network_type = rav::sdp::NetwType::internet;
        connection.address_type = rav::sdp::AddrType::ipv4;
        connection.address = "239.1.16.51";
        connection.ttl = 15;
        REQUIRE(rav::sdp::to_string(connection) == "c=IN IP4 239.1.16.51/15");
    }
}
