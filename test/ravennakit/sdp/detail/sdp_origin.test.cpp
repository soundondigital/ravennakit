// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Project: RAVENNAKIT (RAVENNA / AES67 / ST2110-30 SDK)
 * Copyright (c) 2024-2025 Sound on Digital
 *
 * This file is part of RAVENNAKIT.
 *
 * RAVENNAKIT is dual-licensed:
 *   1) Under the terms of the GNU Affero General Public License as published by
 *      the Free Software Foundation, either version 3 of the License, or
 *      (at your option) any later version (the "AGPL License"); and
 *   2) Under a commercial license from Sound on Digital, for customers who
 *      cannot (or do not wish to) comply with the AGPL License terms.
 *
 * If you obtained this file under the AGPL License, you may redistribute it
 * and/or modify it under the terms of the AGPL License. See the LICENSE
 * file in the project root for details.
 *
 * For commercial licensing, support, and other inquiries, please visit:
 *
 *     https://ravennakit.com
 *
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

    SECTION("To string") {
        rav::sdp::OriginField origin;
        origin.session_id = "13";
        origin.unicast_address = "192.168.15.52";
        origin.network_type = rav::sdp::NetwType::internet;
        origin.address_type = rav::sdp::AddrType::ipv4;
        REQUIRE(rav::sdp::to_string(origin) == "o=- 13 0 IN IP4 192.168.15.52");
    }

    SECTION("Parse origin version as int64") {
        auto origin = rav::sdp::parse_origin("o=- 3989391735 3989391741 IN IP4 10.1.0.111");
        REQUIRE(origin);
        REQUIRE(origin->username == "-");
        REQUIRE(origin->session_id == "3989391735");
        REQUIRE(origin->session_version == 3989391741);
        REQUIRE(origin->network_type == rav::sdp::NetwType::internet);
        REQUIRE(origin->address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(origin->unicast_address == "10.1.0.111");
    }
}
