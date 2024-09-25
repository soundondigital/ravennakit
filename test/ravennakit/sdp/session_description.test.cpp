/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/session_description.hpp"

#include <catch2/catch_all.hpp>

namespace {

constexpr auto k_anubis_sdp =
    "v=0\r\n"
    "o=- 13 0 IN IP4 192.168.15.52\r\n"
    "s=Anubis_610120_13\r\n"
    "c=IN IP4 239.1.15.52/15\r\n"
    "t=0 0\r\n"
    "a=clock-domain:PTPv2 0\r\n"
    "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
    "a=mediaclk:direct=0\r\n"
    "m=audio 5004 RTP/AVP 98\r\n"
    "c=IN IP4 239.1.15.52/15\r\n"
    "a=rtpmap:98 L16/48000/2\r\n"
    "a=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\n"
    "a=clock-domain:PTPv2 0\r\n"
    "a=sync-time:0\r\n"
    "a=framecount:48\r\n"
    "a=palign:0\r\n"
    "a=ptime:1\r\n"
    "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
    "a=mediaclk:direct=0\r\n"
    "a=recvonly\r\n"
    "a=midi-pre2:50040 0,0;0,1\r\n";

}

TEST_CASE("session_description", "[session_description]") {
    SECTION("Test Anubis description") {
        auto result = rav::session_description::parse(k_anubis_sdp);
        REQUIRE(result.is_ok());

        SECTION("Parse a description from an Anubis") {
            REQUIRE(result.get_ok().version() == 0);
        }

        SECTION("Test version") {
            constexpr auto sdp =
                "v=1\r\n"
                "o=- 13 0 IN IP4 192.168.15.52\r\n"
                "s=Anubis_610120_13\r\n";
            REQUIRE(rav::session_description::parse(sdp).is_err());
        }

        SECTION("Test origin") {
            const auto& origin = result.get_ok().origin();
            REQUIRE(origin.username == "-");
            REQUIRE(origin.session_id == "13");
            REQUIRE(origin.session_version == 0);
            REQUIRE(origin.network_type == rav::session_description::netw_type::internet);
            REQUIRE(origin.address_type == rav::session_description::addr_type::ipv4);
            REQUIRE(origin.unicast_address == "192.168.15.52");
        }

        SECTION("Test connection") {
            const auto& connection = result.get_ok().connection_info();
            REQUIRE(connection.has_value());
            REQUIRE(connection->network_type == rav::session_description::netw_type::internet);
            REQUIRE(connection->address_type == rav::session_description::addr_type::ipv4);
            REQUIRE(connection->address == "239.1.15.52");
        }

        SECTION("Test session name") {
            REQUIRE(result.get_ok().session_name() == "Anubis_610120_13");
        }

        SECTION("Test time") {
            auto time = result.get_ok().time_active();
            REQUIRE(time.start_time == 0);
            REQUIRE(time.stop_time == 0);
        }
    }

    SECTION("Test crlf delimited string") {
        constexpr auto crlf =
            "v=0\r\n"
            "o=- 13 0 IN IP4 192.168.15.52\r\n"
            "s=Anubis_610120_13\r\n";
        auto result = rav::session_description::parse(crlf);
        REQUIRE(result.is_ok());
        REQUIRE(result.get_ok().version() == 0);
    }

    SECTION("Test n delimited string") {
        constexpr auto n =
            "v=0\n"
            "o=- 13 0 IN IP4 192.168.15.52\n"
            "s=Anubis_610120_13\n";
        auto result = rav::session_description::parse(n);
        REQUIRE(result.is_ok());
        REQUIRE(result.get_ok().version() == 0);
    }
}

TEST_CASE("session_description | origin_field", "[session_description]") {
    SECTION("Parse origin line") {
        auto result = rav::session_description::origin_field::parse("o=- 13 0 IN IP4 192.168.15.52");
        REQUIRE(result.is_ok());
        auto origin = result.move_ok();
        REQUIRE(origin.username == "-");
        REQUIRE(origin.session_id == "13");
        REQUIRE(origin.session_version == 0);
        REQUIRE(origin.network_type == rav::session_description::netw_type::internet);
        REQUIRE(origin.address_type == rav::session_description::addr_type::ipv4);
        REQUIRE(origin.unicast_address == "192.168.15.52");
    }
}

TEST_CASE("session_description | connection_info_field", "[session_description]") {
    SECTION("Parse connection line") {
        auto result = rav::session_description::connection_info_field::parse("c=IN IP4 239.1.15.52");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::session_description::netw_type::internet);
        REQUIRE(connection.address_type == rav::session_description::addr_type::ipv4);
        REQUIRE(connection.address == "239.1.15.52");
        REQUIRE(connection.ttl.has_value() == false);
        REQUIRE(connection.number_of_addresses.has_value() == false);
    }

    SECTION("Parse connection line with ttl") {
        auto result = rav::session_description::connection_info_field::parse("c=IN IP4 239.1.15.52/15");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::session_description::netw_type::internet);
        REQUIRE(connection.address_type == rav::session_description::addr_type::ipv4);
        REQUIRE(connection.address == "239.1.15.52");
        REQUIRE(connection.ttl.has_value());
        REQUIRE(*connection.ttl == 15);
        REQUIRE(connection.number_of_addresses.has_value() == false);
    }

    SECTION("Parse connection line with ttl and number of addresses") {
        auto result = rav::session_description::connection_info_field::parse("c=IN IP4 239.1.15.52/15/3");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::session_description::netw_type::internet);
        REQUIRE(connection.address_type == rav::session_description::addr_type::ipv4);
        REQUIRE(connection.address == "239.1.15.52");
        REQUIRE(connection.ttl.has_value());
        REQUIRE(*connection.ttl == 15);
        REQUIRE(connection.number_of_addresses.has_value());
        REQUIRE(*connection.number_of_addresses == 3);
    }

    SECTION("Parse ipv6 connection line with number of addresses") {
        auto result = rav::session_description::connection_info_field::parse("c=IN IP6 ff00::db8:0:101/3");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::session_description::netw_type::internet);
        REQUIRE(connection.address_type == rav::session_description::addr_type::ipv6);
        REQUIRE(connection.address == "ff00::db8:0:101");
        REQUIRE(connection.ttl.has_value() == false);
        REQUIRE(connection.number_of_addresses.has_value());
        REQUIRE(*connection.number_of_addresses == 3);
    }

    SECTION("Parse ipv6 connection line with ttl and number of addresses") {
        auto result = rav::session_description::connection_info_field::parse("c=IN IP6 ff00::db8:0:101/127/3");
        REQUIRE(result.is_err());
    }
}

TEST_CASE("session_description | time_field", "[session_description]") {
    SECTION("Test time field") {
        auto result = rav::session_description::time_active_field::parse("t=123456789 987654321");
        REQUIRE(result.is_ok());
        const auto time = result.move_ok();
        REQUIRE(time.start_time == 123456789);
        REQUIRE(time.stop_time == 987654321);
    }

    SECTION("Test invalid time field") {
        auto result = rav::session_description::time_active_field::parse("t=123456789 ");
        REQUIRE(result.is_err());
    }

    SECTION("Test invalid time field") {
        auto result = rav::session_description::time_active_field::parse("t=");
        REQUIRE(result.is_err());
    }
}
