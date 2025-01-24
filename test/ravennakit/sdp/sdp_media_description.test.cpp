/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/sdp_media_description.hpp"

#include <catch2/catch_all.hpp>

#include "ravennakit/core/util.hpp"

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
        auto result = rav::sdp::media_description::parse_new("m=audio 5004/2 RTP/AVP 98 99 100 256");
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

TEST_CASE("media_description | To string") {
    std::string expected =
        "m=audio 5004 RTP/AVP 98\r\n"
        "a=rtpmap:98 L16/44100/2\r\n";

    rav::sdp::media_description md;
    REQUIRE(md.to_string().error() == "media: media type is empty");
    md.set_media_type("audio");
    REQUIRE(md.to_string().error() == "media: port is 0");
    md.set_port(5004);
    REQUIRE(md.to_string().error() == "media: protocol is empty");
    md.set_protocol("RTP/AVP");
    REQUIRE(md.to_string().error() == "media: no formats specified");
    md.add_format({98, "L16", 44100, 2});

    REQUIRE(md.to_string().value() == expected);

    SECTION("Number of ports") {
        auto md2 = md;  // Copy to avoid modifying the original
        md2.set_number_of_ports(2);
        REQUIRE(md2.to_string().value() == "m=audio 5004/2 RTP/AVP 98\r\na=rtpmap:98 L16/44100/2\r\n");
    }

    SECTION("Session information") {
        auto md2 = md;  // Copy to avoid modifying the original
        md2.set_session_information("session info");
        REQUIRE(md2.to_string().value() == "m=audio 5004 RTP/AVP 98\r\ns=session info\r\na=rtpmap:98 L16/44100/2\r\n");
    }

    md.add_connection_info({rav::sdp::netw_type::internet, rav::sdp::addr_type::ipv4, "239.1.16.51", 15, {}});

    expected =
        "m=audio 5004 RTP/AVP 98\r\n"
        "c=IN IP4 239.1.16.51/15\r\n"
        "a=rtpmap:98 L16/44100/2\r\n";

    REQUIRE(md.to_string().value() == expected);

    SECTION("ptime") {
        md.set_ptime(20);
        expected += "a=ptime:20\r\n";
        REQUIRE(md.to_string().value() == expected);
    }

    SECTION("ptime") {
        md.set_ptime(1.0880808);
        expected += "a=ptime:1.09\r\n";
        REQUIRE(md.to_string().value() == expected);
    }

    SECTION("max_ptime") {
        md.set_max_ptime(60);
        expected += "a=maxptime:60\r\n";
        REQUIRE(md.to_string().value() == expected);
    }

    SECTION("max_ptime") {
        md.set_max_ptime(1.0880808);
        expected += "a=maxptime:1.09\r\n";
        REQUIRE(md.to_string().value() == expected);
    }

    SECTION("Media direction") {
        md.set_direction(rav::sdp::media_direction::recvonly);
        expected += "a=recvonly\r\n";
        REQUIRE(md.to_string().value() == expected);
    }

    SECTION("reference clock") {
        md.set_ref_clock(
            {rav::sdp::reference_clock::clock_source::ptp, rav::sdp::reference_clock::ptp_ver::IEEE_1588_2008, "gmid", 1
            }
        );
        expected += "a=ts-refclk:ptp=IEEE1588-2008:gmid:1\r\n";
        REQUIRE(md.to_string().value() == expected);
    }

    SECTION("media clock") {
        md.set_media_clock(
            {rav::sdp::media_clock_source::clock_mode::direct, 5, std::optional<rav::fraction<int>>({48000, 1})}
        );
        expected += "a=mediaclk:direct=5 rate=48000/1\r\n";
        REQUIRE(md.to_string().value() == expected);
    }

    SECTION("RAVENNA clock domain") {
        md.set_clock_domain(rav::sdp::ravenna_clock_domain {rav::sdp::ravenna_clock_domain::sync_source::ptp_v2, 1});
        expected += "a=clock-domain:PTPv2 1\r\n";
        REQUIRE(md.to_string().value() == expected);
    }

    SECTION("RAVENNA sync time") {
        md.set_sync_time(1234);
        expected += "a=sync-time:1234\r\n";
        REQUIRE(md.to_string().value() == expected);
    }

    SECTION("RAVENNA clock deviation") {
        md.set_clock_deviation(std::optional<rav::fraction<unsigned>>({1001, 1000}));
        expected += "a=clock-deviation:1001/1000\r\n";
        REQUIRE(md.to_string().value() == expected);
    }

    SECTION("RAVENNA framecount (legacy)") {
        md.set_framecount(1234);
        expected += "a=framecount:1234\r\n";
        REQUIRE(md.to_string().value() == expected);
    }

    SECTION("Source filters") {
        md.add_source_filter(
            {rav::sdp::filter_mode::include,
             rav::sdp::netw_type::internet,
             rav::sdp::addr_type::ipv4,
             "192.168.1.1",
             {"192.168.1.99"}}
        );
        md.add_source_filter(
            {rav::sdp::filter_mode::include,
             rav::sdp::netw_type::internet,
             rav::sdp::addr_type::ipv4,
             "192.168.2.1",
             {"192.168.2.99", "192.168.2.100"}}
        );
        expected += "a=source-filter: incl IN IP4 192.168.1.1 192.168.1.99\r\n";
        expected += "a=source-filter: incl IN IP4 192.168.2.1 192.168.2.99 192.168.2.100\r\n";
        REQUIRE(md.to_string().value() == expected);
    }
}
