/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_origin.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::sdp::OriginField") {
    SECTION("Parse origin line") {
        auto origin = rav::sdp::parse_origin("o=- 13 0 IN IP4 192.168.15.52");
        REQUIRE(origin);
        REQUIRE(origin->username == "-");
        REQUIRE(origin->session_id == "13");
        REQUIRE(origin->session_version == 0);
        REQUIRE(origin->network_type == rav::sdp::NetwType::internet);
        REQUIRE(origin->address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(origin->unicast_address == "192.168.15.52");
    }

    SECTION("Validate") {
        rav::sdp::OriginField origin;
        REQUIRE(rav::sdp::validate(origin).error() == "origin: session id is empty");
        origin.session_id = "13";
        REQUIRE(rav::sdp::validate(origin).error() == "origin: unicast address is empty");
        origin.unicast_address = "192.168.15.52";
        REQUIRE(rav::sdp::validate(origin).error() == "origin: network type is undefined");
        origin.network_type = rav::sdp::NetwType::internet;
        REQUIRE(rav::sdp::validate(origin).error() == "origin: address type is undefined");
        origin.address_type = rav::sdp::AddrType::ipv4;
        REQUIRE(rav::sdp::validate(origin));
    }

    SECTION("") {
        rav::sdp::OriginField origin;
        origin.session_id = "13";
        origin.unicast_address = "192.168.15.52";
        origin.network_type = rav::sdp::NetwType::internet;
        origin.address_type = rav::sdp::AddrType::ipv4;
        REQUIRE(rav::sdp::to_string(origin) == "o=- 13 0 IN IP4 192.168.15.52");
    }
}
