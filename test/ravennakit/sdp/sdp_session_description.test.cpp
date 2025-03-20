/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/sdp_session_description.hpp"

#include <catch2/catch_all.hpp>

#include "ravennakit/core/util.hpp"

TEST_CASE("session_description") {
    SECTION("Test crlf delimited string") {
        constexpr auto crlf =
            "v=0\r\n"
            "o=- 13 0 IN IP4 192.168.15.52\r\n"
            "s=Anubis_610120_13\r\n";
        auto result = rav::sdp::session_description::parse_new(crlf);
        REQUIRE(result.is_ok());
        REQUIRE(result.get_ok().version() == 0);
    }

    SECTION("Test n delimited string") {
        constexpr auto n =
            "v=0\n"
            "o=- 13 0 IN IP4 192.168.15.52\n"
            "s=Anubis_610120_13\n";
        auto result = rav::sdp::session_description::parse_new(n);
        REQUIRE(result.is_ok());
        REQUIRE(result.get_ok().version() == 0);
    }
}

TEST_CASE("session_description | description from anubis") {
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

    auto result = rav::sdp::session_description::parse_new(k_anubis_sdp);
    REQUIRE(result.is_ok());

    SECTION("Parse a description from an Anubis") {
        REQUIRE(result.get_ok().version() == 0);
    }

    SECTION("Test version") {
        constexpr auto sdp =
            "v=1\r\n"
            "o=- 13 0 IN IP4 192.168.15.52\r\n"
            "s=Anubis_610120_13\r\n";
        REQUIRE(rav::sdp::session_description::parse_new(sdp).is_err());
    }

    SECTION("Test origin") {
        const auto& origin = result.get_ok().origin();
        REQUIRE(origin.username == "-");
        REQUIRE(origin.session_id == "13");
        REQUIRE(origin.session_version == 0);
        REQUIRE(origin.network_type == rav::sdp::netw_type::internet);
        REQUIRE(origin.address_type == rav::sdp::addr_type::ipv4);
        REQUIRE(origin.unicast_address == "192.168.15.52");
    }

    SECTION("Test connection") {
        const auto& connection = result.get_ok().connection_info();
        REQUIRE(connection.has_value());
        REQUIRE(connection->network_type == rav::sdp::netw_type::internet);
        REQUIRE(connection->address_type == rav::sdp::addr_type::ipv4);
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

    SECTION("Test media") {
        const auto& descriptions = result.get_ok().media_descriptions();
        REQUIRE(descriptions.size() == 1);

        const auto& media = descriptions[0];
        REQUIRE(media.media_type() == "audio");
        REQUIRE(media.port() == 5004);
        REQUIRE(media.number_of_ports() == 1);
        REQUIRE(media.protocol() == "RTP/AVP");
        REQUIRE(media.formats().size() == 1);

        auto format = media.formats()[0];
        REQUIRE(format.payload_type == 98);
        REQUIRE(format.encoding_name == "L16");
        REQUIRE(format.clock_rate == 48000);
        REQUIRE(format.num_channels == 2);
        REQUIRE(media.connection_infos().size() == 1);

        const auto& conn = media.connection_infos().back();
        REQUIRE(conn.network_type == rav::sdp::netw_type::internet);
        REQUIRE(conn.address_type == rav::sdp::addr_type::ipv4);
        REQUIRE(conn.address == "239.1.15.52");
        REQUIRE(conn.ttl.has_value() == true);
        REQUIRE(*conn.ttl == 15);
        REQUIRE(static_cast<int64_t>(media.ptime().value()) == 1);

        SECTION("Test refclk on media") {
            const auto& refclk = media.ref_clock();
            REQUIRE(refclk.has_value());
            REQUIRE(refclk->source() == rav::sdp::reference_clock::clock_source::ptp);
            REQUIRE(refclk->ptp_version() == rav::sdp::reference_clock::ptp_ver::IEEE_1588_2008);
            REQUIRE(refclk->gmid() == "00-1D-C1-FF-FE-51-9E-F7");
            REQUIRE(refclk->domain() == 0);
        }

        SECTION("Test sync-time") {
            REQUIRE(media.sync_time().value() == 0);
        }

        SECTION("Test mediaclk on media") {
            const auto& media_clock = media.media_clock().value();
            REQUIRE(media_clock.mode() == rav::sdp::media_clock_source::clock_mode::direct);
            REQUIRE(media_clock.offset().value() == 0);
            REQUIRE_FALSE(media_clock.rate().has_value());
        }

        SECTION("Test source-filter on media") {
            const auto& filters = media.source_filters();
            REQUIRE(filters.size() == 1);
            const auto& filter = filters[0];
            REQUIRE(filter.mode() == rav::sdp::filter_mode::include);
            REQUIRE(filter.network_type() == rav::sdp::netw_type::internet);
            REQUIRE(filter.address_type() == rav::sdp::addr_type::ipv4);
            REQUIRE(filter.dest_address() == "239.1.15.52");
            REQUIRE(filter.src_list().size() == 1);
            REQUIRE(filter.src_list()[0] == "192.168.15.52");
        }

        SECTION("Framecount") {
            REQUIRE(media.framecount() == 48);
        }

        SECTION("Unknown attributes") {
            const auto& attributes = media.attributes();
            REQUIRE(attributes.size() == 2);
            REQUIRE(attributes.at("palign") == "0");
            REQUIRE(attributes.at("midi-pre2") == "50040 0,0;0,1");
        }
    }

    SECTION("Media direction") {
        REQUIRE(result.get_ok().direction() == rav::sdp::media_direction::sendrecv);
    }

    SECTION("Test refclk on session") {
        const auto& refclk = result.get_ok().ref_clock();
        REQUIRE(refclk.has_value());
        REQUIRE(refclk->source() == rav::sdp::reference_clock::clock_source::ptp);
        REQUIRE(refclk->ptp_version() == rav::sdp::reference_clock::ptp_ver::IEEE_1588_2008);
        REQUIRE(refclk->gmid() == "00-1D-C1-FF-FE-51-9E-F7");
        REQUIRE(refclk->domain() == 0);
    }

    SECTION("Test mediaclk attribute") {
        auto media_clock = result.move_ok().media_clock().value();
        REQUIRE(media_clock.mode() == rav::sdp::media_clock_source::clock_mode::direct);
        REQUIRE(media_clock.offset().value() == 0);
        REQUIRE_FALSE(media_clock.rate().has_value());
    }

    SECTION("Test clock-domain") {
        auto clock_domain = result.get_ok().clock_domain().value();
        REQUIRE(clock_domain.source == rav::sdp::ravenna_clock_domain::sync_source::ptp_v2);
        REQUIRE(clock_domain.domain == 0);
    }
}

TEST_CASE("session_description | description from AES67 spec") {
    constexpr auto k_aes67_sdp =
        "v=0\n"
        "o=- 1311738121 1311738121 IN IP4 192.168.1.1\n"
        "s=Stage left I/O\n"
        "c=IN IP4 239.0.0.1/32\n"
        "t=0 0\n"
        "m=audio 5004 RTP/AVP 96\n"
        "i=Channels 1-8\n"
        "a=rtpmap:96 L24/48000/8\n"
        "a=recvonly\n"
        "a=ptime:1\n"
        "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:0\n"
        "a=mediaclk:direct=963214424\n";

    auto result = rav::sdp::session_description::parse_new(k_aes67_sdp);
    REQUIRE(result.is_ok());
    auto session = result.move_ok();
    REQUIRE(session.version() == 0);
    REQUIRE(session.origin().username == "-");
    REQUIRE(session.origin().session_id == "1311738121");
    REQUIRE(session.origin().session_version == 1311738121);
    REQUIRE(session.origin().network_type == rav::sdp::netw_type::internet);
    REQUIRE(session.origin().address_type == rav::sdp::addr_type::ipv4);
    REQUIRE(session.origin().unicast_address == "192.168.1.1");
    REQUIRE(session.session_name() == "Stage left I/O");
    REQUIRE(session.connection_info().has_value());
    REQUIRE(session.connection_info()->network_type == rav::sdp::netw_type::internet);
    REQUIRE(session.connection_info()->address_type == rav::sdp::addr_type::ipv4);
    REQUIRE(session.connection_info()->address == "239.0.0.1");
    REQUIRE(session.connection_info()->ttl == 32);
    REQUIRE(session.time_active().start_time == 0);
    REQUIRE(session.time_active().stop_time == 0);
    REQUIRE(session.media_descriptions().size() == 1);
    const auto& media = session.media_descriptions()[0];
    REQUIRE(media.media_type() == "audio");
    REQUIRE(media.port() == 5004);
    REQUIRE(media.number_of_ports() == 1);
    REQUIRE(media.protocol() == "RTP/AVP");
    REQUIRE(media.formats().size() == 1);
    REQUIRE(media.session_information().value() == "Channels 1-8");
    const auto& format = media.formats()[0];
    REQUIRE(format.payload_type == 96);
    REQUIRE(format.encoding_name == "L24");
    REQUIRE(format.clock_rate == 48000);
    REQUIRE(format.num_channels == 8);
    REQUIRE(media.direction() == rav::sdp::media_direction::recvonly);
    REQUIRE(static_cast<int64_t>(media.ptime().value()) == 1);
    REQUIRE(media.ref_clock().has_value());
    const auto& refclk = media.ref_clock().value();
    REQUIRE(refclk.source() == rav::sdp::reference_clock::clock_source::ptp);
    REQUIRE(refclk.ptp_version() == rav::sdp::reference_clock::ptp_ver::IEEE_1588_2008);
    REQUIRE(refclk.gmid() == "39-A7-94-FF-FE-07-CB-D0");
    REQUIRE(refclk.domain() == 0);
    REQUIRE(media.media_clock().has_value());
    const auto& media_clock = media.media_clock().value();
    REQUIRE(media_clock.mode() == rav::sdp::media_clock_source::clock_mode::direct);
    REQUIRE(media_clock.offset().value() == 963214424);
    REQUIRE_FALSE(media_clock.rate().has_value());
}

TEST_CASE("session_description | description from AES67 spec 2") {
    constexpr auto k_aes67_sdp =
        "v=0\n"
        "o=audio 1311738121 1311738121 IN IP4 192.168.1.1\n"
        "s=Stage left I/O\n"
        "c=IN IP4 192.168.1.1\n"
        "t=0 0\n"
        "m=audio 5004 RTP/AVP 96\n"
        "i=Channels 1-8\n"
        "a=rtpmap:96 L24/48000/8\n"
        "a=sendonly\n"
        "a=ptime:0.250\n"
        "a=ts-refclk:ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:0\n"
        "a=mediaclk:direct=2216659908\n";

    auto result = rav::sdp::session_description::parse_new(k_aes67_sdp);
    if (result.is_err()) {
        FAIL(result.get_err());
    }
    auto session = result.move_ok();
    REQUIRE(session.version() == 0);
    REQUIRE(session.origin().username == "audio");
    REQUIRE(session.origin().session_id == "1311738121");
    REQUIRE(session.origin().session_version == 1311738121);
    REQUIRE(session.origin().network_type == rav::sdp::netw_type::internet);
    REQUIRE(session.origin().address_type == rav::sdp::addr_type::ipv4);
    REQUIRE(session.origin().unicast_address == "192.168.1.1");
    REQUIRE(session.session_name() == "Stage left I/O");
    REQUIRE(session.connection_info().has_value());
    REQUIRE(session.connection_info()->network_type == rav::sdp::netw_type::internet);
    REQUIRE(session.connection_info()->address_type == rav::sdp::addr_type::ipv4);
    REQUIRE(session.connection_info()->address == "192.168.1.1");
    REQUIRE_FALSE(session.connection_info()->ttl.has_value());
    REQUIRE(session.time_active().start_time == 0);
    REQUIRE(session.time_active().stop_time == 0);
    REQUIRE(session.media_descriptions().size() == 1);
    const auto& media = session.media_descriptions()[0];
    REQUIRE(media.media_type() == "audio");
    REQUIRE(media.port() == 5004);
    REQUIRE(media.number_of_ports() == 1);
    REQUIRE(media.protocol() == "RTP/AVP");
    REQUIRE(media.formats().size() == 1);
    REQUIRE(media.session_information().value() == "Channels 1-8");
    const auto& format = media.formats()[0];
    REQUIRE(format.payload_type == 96);
    REQUIRE(format.encoding_name == "L24");
    REQUIRE(format.clock_rate == 48000);
    REQUIRE(format.num_channels == 8);
    REQUIRE(media.direction() == rav::sdp::media_direction::sendonly);
    REQUIRE(rav::is_within(media.ptime().value(), 0.250f, 0.00001f));
    REQUIRE(media.ref_clock().has_value());
    const auto& refclk = media.ref_clock().value();
    REQUIRE(refclk.source() == rav::sdp::reference_clock::clock_source::ptp);
    REQUIRE(refclk.ptp_version() == rav::sdp::reference_clock::ptp_ver::IEEE_1588_2008);
    REQUIRE(refclk.gmid() == "39-A7-94-FF-FE-07-CB-D0");
    REQUIRE(refclk.domain() == 0);
    REQUIRE(media.media_clock().has_value());
    const auto& media_clock = media.media_clock().value();
    REQUIRE(media_clock.mode() == rav::sdp::media_clock_source::clock_mode::direct);
    REQUIRE(media_clock.offset().value() == 2216659908);
    REQUIRE_FALSE(media_clock.rate().has_value());
}

TEST_CASE("session_description | source filters") {
    constexpr auto k_anubis_sdp =
        "v=0\r\n"
        "o=- 13 0 IN IP4 192.168.15.52\r\n"
        "s=Anubis_610120_13\r\n"
        "c=IN IP4 239.1.15.52/15\r\n"
        "t=0 0\r\n"
        "a=clock-domain:PTPv2 0\r\n"
        "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
        "a=mediaclk:direct=0\r\n"
        "a=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\n"
        "m=audio 5004 RTP/AVP 98\r\n"
        "c=IN IP4 239.1.15.52/15\r\n"
        "a=rtpmap:98 L16/48000/2\r\n"
        "a=clock-domain:PTPv2 0\r\n"
        "a=sync-time:0\r\n"
        "a=framecount:48\r\n"
        "a=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\n"
        "a=palign:0\r\n"
        "a=ptime:1\r\n"
        "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
        "a=mediaclk:direct=0\r\n"
        "a=recvonly\r\n"
        "a=midi-pre2:50040 0,0;0,1\r\n";

    auto result = rav::sdp::session_description::parse_new(k_anubis_sdp);
    REQUIRE(result.is_ok());

    SECTION("Session level source filter") {
        const auto& filters = result.get_ok().source_filters();
        REQUIRE(filters.size() == 1);
        const auto& filter = filters[0];
        REQUIRE(filter.mode() == rav::sdp::filter_mode::include);
        REQUIRE(filter.network_type() == rav::sdp::netw_type::internet);
        REQUIRE(filter.address_type() == rav::sdp::addr_type::ipv4);
        REQUIRE(filter.dest_address() == "239.1.15.52");
        const auto& src_list = filter.src_list();
        REQUIRE(src_list.size() == 1);
        REQUIRE(src_list[0] == "192.168.15.52");
    }

    SECTION("Test media") {
        const auto& descriptions = result.get_ok().media_descriptions();
        REQUIRE(descriptions.size() == 1);

        const auto& media = descriptions[0];

        SECTION("Test source-filter on media") {
            const auto& filters = media.source_filters();
            REQUIRE(filters.size() == 1);
            const auto& filter = filters[0];
            REQUIRE(filter.mode() == rav::sdp::filter_mode::include);
            REQUIRE(filter.network_type() == rav::sdp::netw_type::internet);
            REQUIRE(filter.address_type() == rav::sdp::addr_type::ipv4);
            REQUIRE(filter.dest_address() == "239.1.15.52");
            REQUIRE(filter.src_list().size() == 1);
            REQUIRE(filter.src_list()[0] == "192.168.15.52");
        }
    }
}

TEST_CASE("session_description | Unknown attributes") {
    constexpr auto k_anubis_sdp =
        "v=0\r\n"
        "o=- 13 0 IN IP4 192.168.15.52\r\n"
        "s=Anubis_610120_13\r\n"
        "c=IN IP4 239.1.15.52/15\r\n"
        "t=0 0\r\n"
        "a=clock-domain:PTPv2 0\r\n"
        "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
        "a=mediaclk:direct=0\r\n"
        "a=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\n"
        "a=unknown-attribute-session:unknown-attribute-session-value\r\n"
        "m=audio 5004 RTP/AVP 98\r\n"
        "c=IN IP4 239.1.15.52/15\r\n"
        "a=rtpmap:98 L16/48000/2\r\n"
        "a=clock-domain:PTPv2 0\r\n"
        "a=sync-time:0\r\n"
        "a=framecount:48\r\n"
        "a=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\n"
        "a=unknown-attribute-media:unknown-attribute-media-value\r\n"
        "a=palign:0\r\n"
        "a=ptime:1\r\n"
        "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
        "a=mediaclk:direct=0\r\n"
        "a=recvonly\r\n"
        "a=midi-pre2:50040 0,0;0,1\r\n";

    auto result = rav::sdp::session_description::parse_new(k_anubis_sdp);
    REQUIRE(result.is_ok());

    SECTION("Unknown attributes on session") {
        const auto& attributes = result.get_ok().attributes();
        REQUIRE(attributes.size() == 1);
        REQUIRE(attributes.at("unknown-attribute-session") == "unknown-attribute-session-value");
    }

    SECTION("Test media") {
        const auto& descriptions = result.get_ok().media_descriptions();
        REQUIRE(descriptions.size() == 1);

        const auto& media = descriptions[0];

        SECTION("Unknown attributes on media") {
            const auto& attributes = media.attributes();
            REQUIRE(attributes.size() == 3);
            REQUIRE(attributes.at("unknown-attribute-media") == "unknown-attribute-media-value");
            REQUIRE(attributes.at("palign") == "0");
            REQUIRE(attributes.at("midi-pre2") == "50040 0,0;0,1");
        }
    }
}

TEST_CASE("session_description | To string") {
    std::string expected =
        "v=0\r\n"
        "o=- 13 0 IN IP4 192.168.15.52\r\n"
        "s=Anubis Combo LR\r\n"
        "t=0 0\r\n";

    rav::sdp::origin_field origin;
    origin.session_id = "13";
    origin.session_version = 0;
    origin.network_type = rav::sdp::netw_type::internet;
    origin.address_type = rav::sdp::addr_type::ipv4;
    origin.unicast_address = "192.168.15.52";

    rav::sdp::session_description sdp;
    sdp.set_origin(origin);
    sdp.set_session_name("Anubis Combo LR");
    sdp.set_time_active({0, 0});

    REQUIRE(sdp.to_string().value() == expected);

    SECTION("Connection info (optional if media descriptions all have their own connection info)") {
        rav::sdp::connection_info_field connection_info;
        connection_info.network_type = rav::sdp::netw_type::internet;
        connection_info.address_type = rav::sdp::addr_type::ipv4;
        connection_info.address = "239.1.16.51";
        connection_info.ttl = 15;
        sdp.set_connection_info(connection_info);
        expected += "c=IN IP4 239.1.16.51/15\r\n";
        REQUIRE(sdp.to_string().value() == expected);
    }

    REQUIRE(sdp.to_string().value() == expected);

    SECTION("RAVENNA clock-domain attribute") {
        rav::sdp::ravenna_clock_domain clock_domain;
        clock_domain.source = rav::sdp::ravenna_clock_domain::sync_source::ptp_v2;
        clock_domain.domain = 0;
        sdp.set_clock_domain(clock_domain);
        expected += "a=clock-domain:PTPv2 0\r\n";
        REQUIRE(sdp.to_string().value() == expected);
    }

    SECTION("Reference clock attribute") {
        rav::sdp::reference_clock ref_clock(
            rav::sdp::reference_clock::clock_source::ptp, rav::sdp::reference_clock::ptp_ver::IEEE_1588_2008,
            "00-1D-C1-FF-FE-51-9E-F7", 0
        );
        sdp.set_ref_clock(ref_clock);
        expected += "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n";
        REQUIRE(sdp.to_string().value() == expected);
    }

    SECTION("Media direction attribute") {
        sdp.set_media_direction(rav::sdp::media_direction::recvonly);
        expected += "a=recvonly\r\n";
        REQUIRE(sdp.to_string().value() == expected);
    }

    SECTION("Media clock attribute") {
        rav::sdp::media_clock_source media_clock(
            rav::sdp::media_clock_source::clock_mode::direct, 0, rav::fraction<int>({1000, 1001})
        );
        sdp.set_media_clock(media_clock);
        expected += "a=mediaclk:direct=0 rate=1000/1001\r\n";
        REQUIRE(sdp.to_string().value() == expected);
    }

    SECTION("Source filters") {
        rav::sdp::source_filter filter(
            rav::sdp::filter_mode::include, rav::sdp::netw_type::internet, rav::sdp::addr_type::ipv4, "239.1.16.51",
            {"192.168.16.51"}
        );
        sdp.add_source_filter(filter);
        expected += "a=source-filter: incl IN IP4 239.1.16.51 192.168.16.51\r\n";
        REQUIRE(sdp.to_string().value() == expected);
    }

    rav::sdp::media_description md1;
    md1.set_media_type("audio");
    md1.set_port(5004);
    md1.set_number_of_ports(1);
    md1.set_protocol("RTP/AVP");
    md1.add_format({98, "L16", 44100, 2});
    md1.add_connection_info({rav::sdp::netw_type::internet, rav::sdp::addr_type::ipv4, "192.168.1.1", 15, {}});
    md1.set_ptime(20.f);
    md1.set_max_ptime(60.f);
    md1.set_direction(rav::sdp::media_direction::recvonly);
    md1.set_ref_clock(
        {rav::sdp::reference_clock::clock_source::ptp, rav::sdp::reference_clock::ptp_ver::IEEE_1588_2008, "gmid", 1}
    );
    md1.set_media_clock(
        {rav::sdp::media_clock_source::clock_mode::direct, 5, std::optional<rav::fraction<int>>({48000, 1})}
    );
    md1.set_clock_domain(rav::sdp::ravenna_clock_domain {rav::sdp::ravenna_clock_domain::sync_source::ptp_v2, 1});
    md1.set_sync_time(1234);
    md1.set_clock_deviation(std::optional<rav::fraction<unsigned>>({1001, 1000}));
    sdp.add_media_description(md1);

    expected +=
        "m=audio 5004 RTP/AVP 98\r\n"
        "c=IN IP4 192.168.1.1/15\r\n"
        "a=rtpmap:98 L16/44100/2\r\n"
        "a=ptime:20\r\n"
        "a=maxptime:60\r\n"
        "a=recvonly\r\n"
        "a=ts-refclk:ptp=IEEE1588-2008:gmid:1\r\n"
        "a=mediaclk:direct=5 rate=48000/1\r\n"
        "a=clock-domain:PTPv2 1\r\n"
        "a=sync-time:1234\r\n"
        "a=clock-deviation:1001/1000\r\n";

    REQUIRE(sdp.to_string().value() == expected);
}

TEST_CASE("session_description | To string - regenerate Anubis SDP") {
    constexpr auto k_anubis_sdp =
        "v=0\r\n"
        "o=- 13 0 IN IP4 192.168.15.52\r\n"
        "s=Anubis_610120_13\r\n"
        "c=IN IP4 239.1.15.52/15\r\n"
        "t=0 0\r\n"
        "a=clock-domain:PTPv2 0\r\n"
        "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
        "a=mediaclk:direct=0\r\n"
        "a=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\n"
        "a=unknown-attribute-session:unknown-attribute-session-value\r\n"
        "m=audio 5004 RTP/AVP 98\r\n"
        "c=IN IP4 239.1.15.52/15\r\n"
        "a=rtpmap:98 L16/48000/2\r\n"
        "a=clock-domain:PTPv2 0\r\n"
        "a=sync-time:0\r\n"
        "a=framecount:48\r\n"
        "a=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\n"
        "a=unknown-attribute-media:unknown-attribute-media-value\r\n"
        "a=palign:0\r\n"  // Unknown attribute
        "a=ptime:1\r\n"
        "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
        "a=mediaclk:direct=0\r\n"
        "a=recvonly\r\n"
        "a=midi-pre2:50040 0,0;0,1\r\n";

    auto result = rav::sdp::session_description::parse_new(k_anubis_sdp);
    REQUIRE(result.is_ok());

    auto sdp_txt = result.get_ok().to_string().value();
    fmt::println("{}", sdp_txt);

    // Slightly different order
    auto expected =
        "v=0\r\n"
        "o=- 13 0 IN IP4 192.168.15.52\r\n"
        "s=Anubis_610120_13\r\n"
        "t=0 0\r\n"
        "c=IN IP4 239.1.15.52/15\r\n"
        "a=clock-domain:PTPv2 0\r\n"
        "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
        "a=mediaclk:direct=0\r\n"
        "a=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\n"
        "m=audio 5004 RTP/AVP 98\r\n"
        "c=IN IP4 239.1.15.52/15\r\n"
        "a=rtpmap:98 L16/48000/2\r\n"
        "a=ptime:1\r\n"
        "a=recvonly\r\n"
        "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
        "a=mediaclk:direct=0\r\n"
        "a=clock-domain:PTPv2 0\r\n"
        "a=sync-time:0\r\n"
        "a=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\n"
        "a=framecount:48\r\n";

    REQUIRE(sdp_txt == expected);
}
