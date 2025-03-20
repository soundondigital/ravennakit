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

TEST_CASE("media_description | origin_field") {
    SECTION("Parse origin line") {
        auto result = rav::sdp::OriginField::parse_new("o=- 13 0 IN IP4 192.168.15.52");
        REQUIRE(result.is_ok());
        auto origin = result.move_ok();
        REQUIRE(origin.username == "-");
        REQUIRE(origin.session_id == "13");
        REQUIRE(origin.session_version == 0);
        REQUIRE(origin.network_type == rav::sdp::NetwType::internet);
        REQUIRE(origin.address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(origin.unicast_address == "192.168.15.52");
    }

    SECTION("To string") {
        rav::sdp::OriginField origin;
        REQUIRE(origin.to_string().error() == "origin: session id is empty");
        origin.session_id = "13";
        REQUIRE(origin.to_string().error() == "origin: unicast address is empty");
        origin.unicast_address = "192.168.15.52";
        REQUIRE(origin.to_string().error() == "origin: network type is undefined");
        origin.network_type = rav::sdp::NetwType::internet;
        REQUIRE(origin.to_string().error() == "origin: address type is undefined");
        origin.address_type = rav::sdp::AddrType::ipv4;
        REQUIRE(origin.to_string().value() == "o=- 13 0 IN IP4 192.168.15.52");
    }
}
