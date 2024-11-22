/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/media_description.hpp"

#include <catch2/catch_all.hpp>

#include "ravennakit/core/util.hpp"

TEST_CASE("media_description | origin_field") {
    SECTION("Parse origin line") {
        auto result = rav::sdp::origin_field::parse_new("o=- 13 0 IN IP4 192.168.15.52");
        REQUIRE(result.is_ok());
        auto origin = result.move_ok();
        REQUIRE(origin.username == "-");
        REQUIRE(origin.session_id == "13");
        REQUIRE(origin.session_version == 0);
        REQUIRE(origin.network_type == rav::sdp::netw_type::internet);
        REQUIRE(origin.address_type == rav::sdp::addr_type::ipv4);
        REQUIRE(origin.unicast_address == "192.168.15.52");
    }
}

TEST_CASE("media_description | connection_info_field") {
    SECTION("Parse connection line") {
        auto result = rav::sdp::connection_info_field::parse_new("c=IN IP4 239.1.15.52");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::sdp::netw_type::internet);
        REQUIRE(connection.address_type == rav::sdp::addr_type::ipv4);
        REQUIRE(connection.address == "239.1.15.52");
        REQUIRE(connection.ttl.has_value() == false);
        REQUIRE(connection.number_of_addresses.has_value() == false);
    }

    SECTION("Parse connection line with ttl") {
        auto result = rav::sdp::connection_info_field::parse_new("c=IN IP4 239.1.15.52/15");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::sdp::netw_type::internet);
        REQUIRE(connection.address_type == rav::sdp::addr_type::ipv4);
        REQUIRE(connection.address == "239.1.15.52");
        REQUIRE(connection.ttl.has_value());
        REQUIRE(*connection.ttl == 15);
        REQUIRE(connection.number_of_addresses.has_value() == false);
    }

    SECTION("Parse connection line with ttl and number of addresses") {
        auto result = rav::sdp::connection_info_field::parse_new("c=IN IP4 239.1.15.52/15/3");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::sdp::netw_type::internet);
        REQUIRE(connection.address_type == rav::sdp::addr_type::ipv4);
        REQUIRE(connection.address == "239.1.15.52");
        REQUIRE(connection.ttl.has_value());
        REQUIRE(*connection.ttl == 15);
        REQUIRE(connection.number_of_addresses.has_value());
        REQUIRE(*connection.number_of_addresses == 3);
    }

    SECTION("Parse ipv6 connection line with number of addresses") {
        auto result = rav::sdp::connection_info_field::parse_new("c=IN IP6 ff00::db8:0:101/3");
        REQUIRE(result.is_ok());
        auto connection = result.move_ok();
        REQUIRE(connection.network_type == rav::sdp::netw_type::internet);
        REQUIRE(connection.address_type == rav::sdp::addr_type::ipv6);
        REQUIRE(connection.address == "ff00::db8:0:101");
        REQUIRE(connection.ttl.has_value() == false);
        REQUIRE(connection.number_of_addresses.has_value());
        REQUIRE(*connection.number_of_addresses == 3);
    }

    SECTION("Parse ipv6 connection line with ttl and number of addresses (which should fail)") {
        auto result = rav::sdp::connection_info_field::parse_new("c=IN IP6 ff00::db8:0:101/127/3");
        REQUIRE(result.is_err());
    }
}

TEST_CASE("media_description | time_field") {
    SECTION("Test time field") {
        auto result = rav::sdp::time_active_field::parse_new("t=123456789 987654321");
        REQUIRE(result.is_ok());
        const auto time = result.move_ok();
        REQUIRE(time.start_time == 123456789);
        REQUIRE(time.stop_time == 987654321);
    }

    SECTION("Test invalid time field") {
        auto result = rav::sdp::time_active_field::parse_new("t=123456789 ");
        REQUIRE(result.is_err());
    }

    SECTION("Test invalid time field") {
        auto result = rav::sdp::time_active_field::parse_new("t=");
        REQUIRE(result.is_err());
    }
}

TEST_CASE("media_description | media_description") {
    SECTION("Test media field") {
        auto result = rav::sdp::media_description::parse_new("m=audio 5004 RTP/AVP 98");
        REQUIRE(result.is_ok());
        const auto media = result.move_ok();
        REQUIRE(media.media_type() == "audio");
        REQUIRE(media.port() == 5004);
        REQUIRE(media.number_of_ports() == 1);
        REQUIRE(media.protocol() == "RTP/AVP");
        REQUIRE(media.formats().size() == 1);
        auto format = media.formats()[0];
        REQUIRE(format.payload_type == 98);
    }

    SECTION("Test media field with multiple formats") {
        auto result = rav::sdp::media_description::parse_new("m=audio 5004/2 RTP/AVP 98 99 100");
        REQUIRE(result.is_ok());

        auto media = result.move_ok();
        REQUIRE(media.media_type() == "audio");
        REQUIRE(media.port() == 5004);
        REQUIRE(media.number_of_ports() == 2);
        REQUIRE(media.protocol() == "RTP/AVP");
        REQUIRE(media.formats().size() == 3);

        const auto& format1 = media.formats()[0];
        REQUIRE(format1.payload_type == 98);
        REQUIRE(format1.encoding_name.empty());
        REQUIRE(format1.clock_rate == 0);
        REQUIRE(format1.num_channels == 0);

        const auto& format2 = media.formats()[1];
        REQUIRE(format2.payload_type == 99);
        REQUIRE(format2.encoding_name.empty());
        REQUIRE(format2.clock_rate == 0);
        REQUIRE(format2.num_channels == 0);

        const auto& format3 = media.formats()[2];
        REQUIRE(format3.payload_type == 100);
        REQUIRE(format3.encoding_name.empty());
        REQUIRE(format3.clock_rate == 0);
        REQUIRE(format3.num_channels == 0);

        media.parse_attribute("a=rtpmap:98 L16/48000/2");
        REQUIRE(format1.payload_type == 98);
        REQUIRE(format1.encoding_name == "L16");
        REQUIRE(format1.clock_rate == 48000);
        REQUIRE(format1.num_channels == 2);

        media.parse_attribute("a=rtpmap:99 L16/96000/2");
        REQUIRE(format2.payload_type == 99);
        REQUIRE(format2.encoding_name == "L16");
        REQUIRE(format2.clock_rate == 96000);
        REQUIRE(format2.num_channels == 2);

        media.parse_attribute("a=rtpmap:100 L24/44100");
        REQUIRE(format3.payload_type == 100);
        REQUIRE(format3.encoding_name == "L24");
        REQUIRE(format3.clock_rate == 44100);
        REQUIRE(format3.num_channels == 1);
    }

    SECTION("Test media field with multiple formats and an invalid one") {
        auto result = rav::sdp::media_description::parse_new("m=audio 5004/2 RTP/AVP 98 99 100 128");
        REQUIRE(result.is_err());
    }

    SECTION("Test media field direction") {
        auto result = rav::sdp::media_description::parse_new("m=audio 5004/2 RTP/AVP 98 99 100");
        REQUIRE(result.is_ok());
        auto media = result.move_ok();
        REQUIRE_FALSE(media.direction().has_value());
        auto result2 = media.parse_attribute("a=recvonly");
        REQUIRE(result2.is_ok());
        REQUIRE(media.direction().has_value());
        REQUIRE(*media.direction() == rav::sdp::media_direction::recvonly);
    }

    SECTION("Test maxptime attribute") {
        auto result = rav::sdp::media_description::parse_new("m=audio 5004/2 RTP/AVP 98 99 100");
        REQUIRE(result.is_ok());
        auto media = result.move_ok();
        REQUIRE_FALSE(media.max_ptime().has_value());
        media.parse_attribute("a=maxptime:60.5");
        REQUIRE(media.max_ptime().has_value());
        REQUIRE(rav::util::is_within(*media.max_ptime(), 60.5, 0.0001));
    }

    SECTION("Test mediaclk attribute") {
        auto result = rav::sdp::media_description::parse_new("m=audio 5004/2 RTP/AVP 98 99 100");
        REQUIRE(result.is_ok());
        auto media = result.move_ok();
        REQUIRE_FALSE(media.media_clock().has_value());
        media.parse_attribute("a=mediaclk:direct=5 rate=48000/1");
        REQUIRE(media.media_clock().has_value());
        const auto& clock = media.media_clock().value();
        REQUIRE(clock.mode() == rav::sdp::media_clock_source::clock_mode::direct);
        REQUIRE(clock.offset().value() == 5);
        REQUIRE(clock.rate().has_value());
        REQUIRE(clock.rate().value().numerator == 48000);
        REQUIRE(clock.rate().value().denominator == 1);
    }

    SECTION("Test clock-deviation attribute") {
        auto result = rav::sdp::media_description::parse_new("m=audio 5004/2 RTP/AVP 98 99 100");
        REQUIRE(result.is_ok());
        auto media = result.move_ok();
        REQUIRE_FALSE(media.media_clock().has_value());
        media.parse_attribute("a=clock-deviation:1001/1000");
        REQUIRE(media.clock_deviation().has_value());
        REQUIRE(media.clock_deviation().value().numerator == 1001);
        REQUIRE(media.clock_deviation().value().denominator == 1000);
    }
}

TEST_CASE("media_description | format") {
    SECTION("98/L16/48000/2") {
        auto result = rav::sdp::format::parse_new("98 L16/48000/2");
        REQUIRE(result.is_ok());
        auto fmt = result.move_ok();
        REQUIRE(fmt.payload_type == 98);
        REQUIRE(fmt.encoding_name == "L16");
        REQUIRE(fmt.clock_rate == 48000);
        REQUIRE(fmt.num_channels == 2);
        REQUIRE(fmt.bytes_per_sample() == 2);
    }

    SECTION("98/L16/48000/4") {
        auto result = rav::sdp::format::parse_new("98 L16/48000/4");
        REQUIRE(result.is_ok());
        auto fmt = result.move_ok();
        REQUIRE(fmt.payload_type == 98);
        REQUIRE(fmt.encoding_name == "L16");
        REQUIRE(fmt.clock_rate == 48000);
        REQUIRE(fmt.num_channels == 4);
        REQUIRE(fmt.bytes_per_sample() == 2);
    }

    SECTION("98/L24/48000/2") {
        auto result = rav::sdp::format::parse_new("98 L24/48000/2");
        REQUIRE(result.is_ok());
        auto fmt = result.move_ok();
        REQUIRE(fmt.payload_type == 98);
        REQUIRE(fmt.encoding_name == "L24");
        REQUIRE(fmt.clock_rate == 48000);
        REQUIRE(fmt.num_channels == 2);
        REQUIRE(fmt.bytes_per_sample() == 3);
    }

    SECTION("98/L32/48000/2") {
        auto result = rav::sdp::format::parse_new("98 L32/48000/2");
        REQUIRE(result.is_ok());
        auto fmt = result.move_ok();
        REQUIRE(fmt.payload_type == 98);
        REQUIRE(fmt.encoding_name == "L32");
        REQUIRE(fmt.clock_rate == 48000);
        REQUIRE(fmt.num_channels == 2);
        REQUIRE(fmt.bytes_per_sample() == 4);
    }

    SECTION("98/NA/48000/2") {
        auto result = rav::sdp::format::parse_new("98 NA/48000/2");
        REQUIRE(result.is_ok());
        auto fmt = result.move_ok();
        REQUIRE(fmt.payload_type == 98);
        REQUIRE(fmt.encoding_name == "NA");
        REQUIRE(fmt.clock_rate == 48000);
        REQUIRE(fmt.num_channels == 2);
        REQUIRE_FALSE(fmt.bytes_per_sample().has_value());
    }
}
