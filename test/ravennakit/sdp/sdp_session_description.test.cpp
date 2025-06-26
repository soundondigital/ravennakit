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

TEST_CASE("rav::sdp::SessionDescription") {
    SECTION("Test crlf delimited string") {
        constexpr auto crlf =
            "v=0\r\n"
            "o=- 13 0 IN IP4 192.168.15.52\r\n"
            "s=Anubis_610120_13\r\n";
        auto result = rav::sdp::parse_session_description(crlf);
        REQUIRE(result);
        REQUIRE(result->version == 0);
    }

    SECTION("Test n delimited string") {
        constexpr auto n =
            "v=0\n"
            "o=- 13 0 IN IP4 192.168.15.52\n"
            "s=Anubis_610120_13\n";
        auto result = rav::sdp::parse_session_description(n);
        REQUIRE(result);
        REQUIRE(result->version == 0);
    }

    SECTION("Test string without newline") {
        constexpr auto str = "bbb";
        auto result = rav::sdp::parse_session_description(str);
        REQUIRE_FALSE(result);
    }

    SECTION("session_description | description from anubis") {
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

        auto result = rav::sdp::parse_session_description(k_anubis_sdp);
        REQUIRE(result);

        SECTION("Parse a description from an Anubis") {
            REQUIRE(result->version == 0);
        }

        SECTION("Test version") {
            constexpr auto sdp =
                "v=1\r\n"
                "o=- 13 0 IN IP4 192.168.15.52\r\n"
                "s=Anubis_610120_13\r\n";
            REQUIRE_FALSE(rav::sdp::parse_session_description(sdp));
        }

        SECTION("Test origin") {
            const auto& origin = result->origin;
            REQUIRE(origin.username == "-");
            REQUIRE(origin.session_id == "13");
            REQUIRE(origin.session_version == 0);
            REQUIRE(origin.network_type == rav::sdp::NetwType::internet);
            REQUIRE(origin.address_type == rav::sdp::AddrType::ipv4);
            REQUIRE(origin.unicast_address == "192.168.15.52");
        }

        SECTION("Test connection") {
            const auto& connection = result->connection_info;
            REQUIRE(connection.has_value());
            REQUIRE(connection->network_type == rav::sdp::NetwType::internet);
            REQUIRE(connection->address_type == rav::sdp::AddrType::ipv4);
            REQUIRE(connection->address == "239.1.15.52");
        }

        SECTION("Test session name") {
            REQUIRE(result->session_name == "Anubis_610120_13");
        }

        SECTION("Test time") {
            auto time = result->time_active;
            REQUIRE(time.start_time == 0);
            REQUIRE(time.stop_time == 0);
        }

        SECTION("Test media") {
            const auto& descriptions = result->media_descriptions;
            REQUIRE(descriptions.size() == 1);

            const auto& media = descriptions[0];
            REQUIRE(media.media_type == "audio");
            REQUIRE(media.port == 5004);
            REQUIRE(media.number_of_ports == 1);
            REQUIRE(media.protocol == "RTP/AVP");
            REQUIRE(media.formats.size() == 1);

            auto format = media.formats[0];
            REQUIRE(format.payload_type == 98);
            REQUIRE(format.encoding_name == "L16");
            REQUIRE(format.clock_rate == 48000);
            REQUIRE(format.num_channels == 2);
            REQUIRE(media.connection_infos.size() == 1);

            const auto& conn = media.connection_infos.back();
            REQUIRE(conn.network_type == rav::sdp::NetwType::internet);
            REQUIRE(conn.address_type == rav::sdp::AddrType::ipv4);
            REQUIRE(conn.address == "239.1.15.52");
            REQUIRE(conn.ttl.has_value() == true);
            REQUIRE(*conn.ttl == 15);
            REQUIRE(static_cast<int64_t>(media.ptime.value()) == 1);

            SECTION("Test refclk on media") {
                const auto& refclk = media.reference_clock;
                REQUIRE(refclk.has_value());
                REQUIRE(refclk->source_ == rav::sdp::ReferenceClock::ClockSource::ptp);
                REQUIRE(refclk->ptp_version_ == rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008);
                REQUIRE(refclk->gmid_ == "00-1D-C1-FF-FE-51-9E-F7");
                REQUIRE(refclk->domain_ == 0);
            }

            SECTION("Test sync-time") {
                REQUIRE(media.ravenna_sync_time.value() == 0);
            }

            SECTION("Test mediaclk on media") {
                const auto& media_clock = media.media_clock.value();
                REQUIRE(media_clock.mode == rav::sdp::MediaClockSource::ClockMode::direct);
                REQUIRE(media_clock.offset.value() == 0);
                REQUIRE_FALSE(media_clock.rate.has_value());
            }

            SECTION("Test source-filter on media") {
                const auto& filters = media.source_filters;
                REQUIRE(filters.size() == 1);
                const auto& filter = filters[0];
                REQUIRE(filter.mode == rav::sdp::FilterMode::include);
                REQUIRE(filter.net_type == rav::sdp::NetwType::internet);
                REQUIRE(filter.addr_type == rav::sdp::AddrType::ipv4);
                REQUIRE(filter.dest_address == "239.1.15.52");
                REQUIRE(filter.src_list.size() == 1);
                REQUIRE(filter.src_list[0] == "192.168.15.52");
            }

            SECTION("Framecount") {
                REQUIRE(media.ravenna_framecount == 48);
            }

            SECTION("Unknown attributes") {
                const auto& attributes = media.attributes;
                REQUIRE(attributes.size() == 2);
                REQUIRE(attributes.at("palign") == "0");
                REQUIRE(attributes.at("midi-pre2") == "50040 0,0;0,1");
            }
        }

        SECTION("Media direction") {
            REQUIRE(!result->media_direction.has_value());
        }

        SECTION("Test refclk on session") {
            const auto& refclk = result->reference_clock;
            REQUIRE(refclk.has_value());
            REQUIRE(refclk->source_ == rav::sdp::ReferenceClock::ClockSource::ptp);
            REQUIRE(refclk->ptp_version_ == rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008);
            REQUIRE(refclk->gmid_ == "00-1D-C1-FF-FE-51-9E-F7");
            REQUIRE(refclk->domain_ == 0);
        }

        SECTION("Test mediaclk attribute") {
            auto media_clock = result->media_clock.value();
            REQUIRE(media_clock.mode == rav::sdp::MediaClockSource::ClockMode::direct);
            REQUIRE(media_clock.offset.value() == 0);
            REQUIRE_FALSE(media_clock.rate.has_value());
        }

        SECTION("Test clock-domain") {
            auto clock_domain = result->ravenna_clock_domain.value();
            REQUIRE(clock_domain.source == rav::sdp::RavennaClockDomain::SyncSource::ptp_v2);
            REQUIRE(clock_domain.domain == 0);
        }
    }

    SECTION("session_description | description from AES67 spec") {
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

        auto result = rav::sdp::parse_session_description(k_aes67_sdp);
        REQUIRE(result);
        auto session = *result;
        REQUIRE(session.version == 0);
        REQUIRE(session.origin.username == "-");
        REQUIRE(session.origin.session_id == "1311738121");
        REQUIRE(session.origin.session_version == 1311738121);
        REQUIRE(session.origin.network_type == rav::sdp::NetwType::internet);
        REQUIRE(session.origin.address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(session.origin.unicast_address == "192.168.1.1");
        REQUIRE(session.session_name == "Stage left I/O");
        REQUIRE(session.connection_info.has_value());
        REQUIRE(session.connection_info->network_type == rav::sdp::NetwType::internet);
        REQUIRE(session.connection_info->address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(session.connection_info->address == "239.0.0.1");
        REQUIRE(session.connection_info->ttl == 32);
        REQUIRE(session.time_active.start_time == 0);
        REQUIRE(session.time_active.stop_time == 0);
        REQUIRE(session.media_descriptions.size() == 1);
        const auto& media = session.media_descriptions[0];
        REQUIRE(media.media_type == "audio");
        REQUIRE(media.port == 5004);
        REQUIRE(media.number_of_ports == 1);
        REQUIRE(media.protocol == "RTP/AVP");
        REQUIRE(media.formats.size() == 1);
        REQUIRE(media.session_information.value() == "Channels 1-8");
        const auto& format = media.formats[0];
        REQUIRE(format.payload_type == 96);
        REQUIRE(format.encoding_name == "L24");
        REQUIRE(format.clock_rate == 48000);
        REQUIRE(format.num_channels == 8);
        REQUIRE(media.media_direction == rav::sdp::MediaDirection::recvonly);
        REQUIRE(static_cast<int64_t>(media.ptime.value()) == 1);
        REQUIRE(media.reference_clock.has_value());
        const auto& refclk = media.reference_clock.value();
        REQUIRE(refclk.source_ == rav::sdp::ReferenceClock::ClockSource::ptp);
        REQUIRE(refclk.ptp_version_ == rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008);
        REQUIRE(refclk.gmid_ == "39-A7-94-FF-FE-07-CB-D0");
        REQUIRE(refclk.domain_ == 0);
        REQUIRE(media.media_clock.has_value());
        const auto& media_clock = media.media_clock.value();
        REQUIRE(media_clock.mode == rav::sdp::MediaClockSource::ClockMode::direct);
        REQUIRE(media_clock.offset.value() == 963214424);
        REQUIRE_FALSE(media_clock.rate.has_value());
    }

    SECTION("session_description | description from AES67 spec 2") {
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

        auto result = rav::sdp::parse_session_description(k_aes67_sdp);
        if (!result) {
            FAIL(result.error());
        }
        auto session = std::move(*result);
        REQUIRE(session.version == 0);
        REQUIRE(session.origin.username == "audio");
        REQUIRE(session.origin.session_id == "1311738121");
        REQUIRE(session.origin.session_version == 1311738121);
        REQUIRE(session.origin.network_type == rav::sdp::NetwType::internet);
        REQUIRE(session.origin.address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(session.origin.unicast_address == "192.168.1.1");
        REQUIRE(session.session_name == "Stage left I/O");
        REQUIRE(session.connection_info.has_value());
        REQUIRE(session.connection_info->network_type == rav::sdp::NetwType::internet);
        REQUIRE(session.connection_info->address_type == rav::sdp::AddrType::ipv4);
        REQUIRE(session.connection_info->address == "192.168.1.1");
        REQUIRE_FALSE(session.connection_info->ttl.has_value());
        REQUIRE(session.time_active.start_time == 0);
        REQUIRE(session.time_active.stop_time == 0);
        REQUIRE(session.media_descriptions.size() == 1);
        const auto& media = session.media_descriptions[0];
        REQUIRE(media.media_type == "audio");
        REQUIRE(media.port == 5004);
        REQUIRE(media.number_of_ports == 1);
        REQUIRE(media.protocol == "RTP/AVP");
        REQUIRE(media.formats.size() == 1);
        REQUIRE(media.session_information.value() == "Channels 1-8");
        const auto& format = media.formats[0];
        REQUIRE(format.payload_type == 96);
        REQUIRE(format.encoding_name == "L24");
        REQUIRE(format.clock_rate == 48000);
        REQUIRE(format.num_channels == 8);
        REQUIRE(media.media_direction == rav::sdp::MediaDirection::sendonly);
        REQUIRE(rav::is_within(media.ptime.value(), 0.250f, 0.00001f));
        REQUIRE(media.reference_clock.has_value());
        const auto& refclk = media.reference_clock.value();
        REQUIRE(refclk.source_ == rav::sdp::ReferenceClock::ClockSource::ptp);
        REQUIRE(refclk.ptp_version_ == rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008);
        REQUIRE(refclk.gmid_ == "39-A7-94-FF-FE-07-CB-D0");
        REQUIRE(refclk.domain_ == 0);
        REQUIRE(media.media_clock.has_value());
        const auto& media_clock = media.media_clock.value();
        REQUIRE(media_clock.mode == rav::sdp::MediaClockSource::ClockMode::direct);
        REQUIRE(media_clock.offset.value() == 2216659908);
        REQUIRE_FALSE(media_clock.rate.has_value());
    }

    SECTION("session_description | source filters") {
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

        auto result = rav::sdp::parse_session_description(k_anubis_sdp);
        REQUIRE(result);

        SECTION("Session level source filter") {
            const auto& filters = result->source_filters;
            REQUIRE(filters.size() == 1);
            const auto& filter = filters[0];
            REQUIRE(filter.mode == rav::sdp::FilterMode::include);
            REQUIRE(filter.net_type == rav::sdp::NetwType::internet);
            REQUIRE(filter.addr_type == rav::sdp::AddrType::ipv4);
            REQUIRE(filter.dest_address == "239.1.15.52");
            const auto& src_list = filter.src_list;
            REQUIRE(src_list.size() == 1);
            REQUIRE(src_list[0] == "192.168.15.52");
        }

        SECTION("Test media") {
            const auto& descriptions = result->media_descriptions;
            REQUIRE(descriptions.size() == 1);

            const auto& media = descriptions[0];

            SECTION("Test source-filter on media") {
                const auto& filters = media.source_filters;
                REQUIRE(filters.size() == 1);
                const auto& filter = filters[0];
                REQUIRE(filter.mode == rav::sdp::FilterMode::include);
                REQUIRE(filter.net_type == rav::sdp::NetwType::internet);
                REQUIRE(filter.addr_type == rav::sdp::AddrType::ipv4);
                REQUIRE(filter.dest_address == "239.1.15.52");
                REQUIRE(filter.src_list.size() == 1);
                REQUIRE(filter.src_list[0] == "192.168.15.52");
            }
        }
    }

    SECTION("session_description | Unknown attributes") {
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

        auto result = rav::sdp::parse_session_description(k_anubis_sdp);
        REQUIRE(result);

        SECTION("Unknown attributes on session") {
            REQUIRE(result->attributes.size() == 1);
            REQUIRE(result->attributes.at("unknown-attribute-session") == "unknown-attribute-session-value");
        }

        SECTION("Test media") {
            REQUIRE(result->media_descriptions.size() == 1);
            const auto& media = result->media_descriptions[0];

            SECTION("Unknown attributes on media") {
                const auto& attributes = media.attributes;
                REQUIRE(attributes.size() == 3);
                REQUIRE(attributes.at("unknown-attribute-media") == "unknown-attribute-media-value");
                REQUIRE(attributes.at("palign") == "0");
                REQUIRE(attributes.at("midi-pre2") == "50040 0,0;0,1");
            }
        }
    }

    SECTION("session_description | To string") {
        std::string expected =
            "v=0\r\n"
            "o=- 13 0 IN IP4 192.168.15.52\r\n"
            "s=Anubis Combo LR\r\n"
            "t=0 0\r\n";

        rav::sdp::OriginField origin;
        origin.session_id = "13";
        origin.session_version = 0;
        origin.network_type = rav::sdp::NetwType::internet;
        origin.address_type = rav::sdp::AddrType::ipv4;
        origin.unicast_address = "192.168.15.52";

        rav::sdp::SessionDescription sdp;
        sdp.origin = origin;
        sdp.session_name = "Anubis Combo LR";
        sdp.time_active = {0, 0};

        REQUIRE(rav::sdp::to_string(sdp) == expected);

        SECTION("Connection info (optional if media descriptions all have their own connection info)") {
            rav::sdp::ConnectionInfoField connection_info;
            connection_info.network_type = rav::sdp::NetwType::internet;
            connection_info.address_type = rav::sdp::AddrType::ipv4;
            connection_info.address = "239.1.16.51";
            connection_info.ttl = 15;
            sdp.connection_info = connection_info;
            expected += "c=IN IP4 239.1.16.51/15\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        REQUIRE(rav::sdp::to_string(sdp) == expected);

        SECTION("RAVENNA clock-domain attribute") {
            rav::sdp::RavennaClockDomain clock_domain;
            clock_domain.source = rav::sdp::RavennaClockDomain::SyncSource::ptp_v2;
            clock_domain.domain = 0;
            sdp.ravenna_clock_domain = clock_domain;
            expected += "a=clock-domain:PTPv2 0\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        SECTION("Reference clock attribute") {
            rav::sdp::ReferenceClock ref_clock {
                rav::sdp::ReferenceClock::ClockSource::ptp, rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008,
                "00-1D-C1-FF-FE-51-9E-F7", 0
            };
            sdp.reference_clock = ref_clock;
            expected += "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        SECTION("Media direction attribute") {
            sdp.media_direction = rav::sdp::MediaDirection::recvonly;
            expected += "a=recvonly\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        SECTION("Media clock attribute") {
            rav::sdp::MediaClockSource media_clock {
                rav::sdp::MediaClockSource::ClockMode::direct, 0, rav::Fraction<int>({1000, 1001})
            };
            sdp.media_clock = media_clock;
            expected += "a=mediaclk:direct=0 rate=1000/1001\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        SECTION("Source filters") {
            rav::sdp::SourceFilter filter {
                rav::sdp::FilterMode::include,
                rav::sdp::NetwType::internet,
                rav::sdp::AddrType::ipv4,
                "239.1.16.51",
                {"192.168.16.51"}
            };
            sdp.add_or_update_source_filter(filter);
            expected += "a=source-filter: incl IN IP4 239.1.16.51 192.168.16.51\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        rav::sdp::MediaDescription md1;
        md1.media_type = "audio";
        md1.port = 5004;
        md1.number_of_ports = 1;
        md1.protocol = "RTP/AVP";
        md1.add_or_update_format({98, "L16", 44100, 2});
        md1.connection_infos.push_back({rav::sdp::NetwType::internet, rav::sdp::AddrType::ipv4, "192.168.1.1", 15, {}});
        md1.ptime = 20.f;
        md1.max_ptime = 60.f;
        md1.media_direction = rav::sdp::MediaDirection::recvonly;
        md1.reference_clock = {
            rav::sdp::ReferenceClock::ClockSource::ptp, rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008, "gmid", 1
        };
        md1.media_clock = {
            rav::sdp::MediaClockSource::ClockMode::direct, 5, std::optional<rav::Fraction<int>>({48000, 1})
        };
        md1.ravenna_clock_domain = rav::sdp::RavennaClockDomain {rav::sdp::RavennaClockDomain::SyncSource::ptp_v2, 1};
        md1.ravenna_sync_time = 1234;
        md1.ravenna_clock_deviation = std::optional<rav::Fraction<unsigned>>({1001, 1000});
        sdp.media_descriptions.push_back(md1);

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

        REQUIRE(rav::sdp::to_string(sdp) == expected);
    }

    SECTION("session_description | To string SPS") {
        std::string expected =
            "v=0\r\n"
            "o=- 13 0 IN IP4 192.168.15.52\r\n"
            "s=Anubis Combo LR\r\n"
            "t=0 0\r\n";

        rav::sdp::OriginField origin;
        origin.session_id = "13";
        origin.session_version = 0;
        origin.network_type = rav::sdp::NetwType::internet;
        origin.address_type = rav::sdp::AddrType::ipv4;
        origin.unicast_address = "192.168.15.52";

        rav::sdp::SessionDescription sdp;
        sdp.origin = origin;
        sdp.session_name = "Anubis Combo LR";
        sdp.time_active = {0, 0};

        REQUIRE(rav::sdp::to_string(sdp) == expected);

        rav::sdp::Group group;
        group.type = rav::sdp::Group::Type::dup;
        group.tags.push_back("primary");
        group.tags.push_back("secondary");

        sdp.group = group;

        expected += "a=group:DUP primary secondary\r\n";

        SECTION("Connection info (optional if media descriptions all have their own connection info)") {
            rav::sdp::ConnectionInfoField connection_info;
            connection_info.network_type = rav::sdp::NetwType::internet;
            connection_info.address_type = rav::sdp::AddrType::ipv4;
            connection_info.address = "239.1.16.51";
            connection_info.ttl = 15;
            sdp.connection_info = connection_info;
            expected += "c=IN IP4 239.1.16.51/15\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        REQUIRE(rav::sdp::to_string(sdp) == expected);

        SECTION("RAVENNA clock-domain attribute") {
            rav::sdp::RavennaClockDomain clock_domain;
            clock_domain.source = rav::sdp::RavennaClockDomain::SyncSource::ptp_v2;
            clock_domain.domain = 0;
            sdp.ravenna_clock_domain = clock_domain;
            expected += "a=clock-domain:PTPv2 0\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        SECTION("Reference clock attribute") {
            rav::sdp::ReferenceClock ref_clock {
                rav::sdp::ReferenceClock::ClockSource::ptp, rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008,
                "00-1D-C1-FF-FE-51-9E-F7", 0
            };
            sdp.reference_clock = ref_clock;
            expected += "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        SECTION("Media direction attribute") {
            sdp.media_direction = rav::sdp::MediaDirection::recvonly;
            expected += "a=recvonly\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        SECTION("Media clock attribute") {
            rav::sdp::MediaClockSource media_clock {
                rav::sdp::MediaClockSource::ClockMode::direct, 0, rav::Fraction<int>({1000, 1001})
            };
            sdp.media_clock = media_clock;
            expected += "a=mediaclk:direct=0 rate=1000/1001\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        SECTION("Source filters") {
            rav::sdp::SourceFilter filter {
                rav::sdp::FilterMode::include,
                rav::sdp::NetwType::internet,
                rav::sdp::AddrType::ipv4,
                "239.1.16.51",
                {"192.168.16.51"}
            };
            sdp.add_or_update_source_filter(filter);
            expected += "a=source-filter: incl IN IP4 239.1.16.51 192.168.16.51\r\n";
            REQUIRE(rav::sdp::to_string(sdp) == expected);
        }

        rav::sdp::MediaDescription primary;
        primary.media_type = "audio";
        primary.port = 5004;
        primary.number_of_ports = 1;
        primary.protocol = "RTP/AVP";
        primary.add_or_update_format({98, "L16", 44100, 2});
        primary.connection_infos.push_back(
            {rav::sdp::NetwType::internet, rav::sdp::AddrType::ipv4, "192.168.1.1", 15, {}}
        );
        primary.ptime = 20.f;
        primary.max_ptime = 60.f;
        primary.media_direction = rav::sdp::MediaDirection::recvonly;
        primary.reference_clock = {
            rav::sdp::ReferenceClock::ClockSource::ptp, rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008, "gmid", 1
        };
        primary.media_clock = {
            rav::sdp::MediaClockSource::ClockMode::direct, 5, std::optional<rav::Fraction<int>>({48000, 1})
        };
        primary.ravenna_clock_domain =
            rav::sdp::RavennaClockDomain {rav::sdp::RavennaClockDomain::SyncSource::ptp_v2, 1};
        primary.ravenna_sync_time = 1234;
        primary.ravenna_clock_deviation = std::optional<rav::Fraction<unsigned>>({1001, 1000});
        primary.mid = "primary";
        sdp.media_descriptions.push_back(primary);

        expected +=
            "m=audio 5004 RTP/AVP 98\r\n"
            "c=IN IP4 192.168.1.1/15\r\n"
            "a=rtpmap:98 L16/44100/2\r\n"
            "a=ptime:20\r\n"
            "a=maxptime:60\r\n"
            "a=mid:primary\r\n"
            "a=recvonly\r\n"
            "a=ts-refclk:ptp=IEEE1588-2008:gmid:1\r\n"
            "a=mediaclk:direct=5 rate=48000/1\r\n"
            "a=clock-domain:PTPv2 1\r\n"
            "a=sync-time:1234\r\n"
            "a=clock-deviation:1001/1000\r\n";

        rav::sdp::MediaDescription secondary;
        secondary.media_type = "audio";
        secondary.port = 5004;
        secondary.number_of_ports = 1;
        secondary.protocol = "RTP/AVP";
        secondary.add_or_update_format({98, "L16", 44100, 2});
        secondary.connection_infos.push_back(
            {rav::sdp::NetwType::internet, rav::sdp::AddrType::ipv4, "192.168.1.2", 15, {}}
        );
        secondary.ptime = 20.f;
        secondary.max_ptime = 60.f;
        secondary.media_direction = rav::sdp::MediaDirection::recvonly;
        secondary.reference_clock = {
            rav::sdp::ReferenceClock::ClockSource::ptp, rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008, "gmid", 1
        };
        secondary.media_clock = {
            rav::sdp::MediaClockSource::ClockMode::direct, 5, std::optional<rav::Fraction<int>>({48000, 1})
        };
        secondary.ravenna_clock_domain =
            rav::sdp::RavennaClockDomain {rav::sdp::RavennaClockDomain::SyncSource::ptp_v2, 1};
        secondary.ravenna_sync_time = 1234;
        secondary.ravenna_clock_deviation = std::optional<rav::Fraction<unsigned>>({1001, 1000});
        secondary.mid = "secondary";
        sdp.media_descriptions.push_back(secondary);

        expected +=
            "m=audio 5004 RTP/AVP 98\r\n"
            "c=IN IP4 192.168.1.2/15\r\n"
            "a=rtpmap:98 L16/44100/2\r\n"
            "a=ptime:20\r\n"
            "a=maxptime:60\r\n"
            "a=mid:secondary\r\n"
            "a=recvonly\r\n"
            "a=ts-refclk:ptp=IEEE1588-2008:gmid:1\r\n"
            "a=mediaclk:direct=5 rate=48000/1\r\n"
            "a=clock-domain:PTPv2 1\r\n"
            "a=sync-time:1234\r\n"
            "a=clock-deviation:1001/1000\r\n";

        REQUIRE(rav::sdp::to_string(sdp) == expected);
    }

    SECTION("session_description | To string - regenerate Anubis SDP") {
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

        auto result = rav::sdp::parse_session_description(k_anubis_sdp);
        REQUIRE(result);

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

        REQUIRE(rav::sdp::to_string(*result) == expected);
    }

    SECTION("session_description | SPS description from Mic-8") {
        constexpr auto k_mic8_sdp =
            "v=0\r\n"
            "o=- 1731086923289383 0 IN IP4 192.168.4.8\r\n"
            "s=MADI-1\r\n"
            "t=0 0\r\n"
            "a=group:DUP primary secondary\r\n"
            "a=clock-domain:PTPv2 0\r\n"
            "a=sync-time:0\r\n"
            "a=ts-refclk:ptp=IEEE1588-2008:00-0B-72-FF-FE-07-DC-FC:0\r\n"
            "a=mediaclk:direct=0\r\n"
            "m=audio 5004 RTP/AVP 98\r\n"
            "c=IN IP4 239.3.8.1/31\r\n"
            "a=source-filter: incl IN IP4 239.3.8.1 192.168.16.52\r\n"
            "a=recvonly\r\n"
            "a=rtpmap:98 L24/48000/64\r\n"
            "a=framecount:6\r\n"
            "a=ptime:0.12\r\n"
            "a=mid:primary\r\n"
            "a=clock-domain:PTPv2 0\r\n"
            "a=sync-time:0\r\n"
            "a=ts-refclk:ptp=IEEE1588-2008:00-0B-72-FF-FE-07-DC-FC:0\r\n"
            "a=mediaclk:direct=0\r\n"
            "m=audio 5004 RTP/AVP 98\r\n"
            "c=IN IP4 239.4.8.2/31\r\n"
            "a=source-filter: incl IN IP4 239.4.8.2 192.168.4.8\r\n"
            "a=recvonly\r\n"
            "a=rtpmap:98 L24/48000/64\r\n"
            "a=framecount:6\r\n"
            "a=ptime:0.12\r\n"
            "a=mid:secondary\r\n"
            "a=clock-domain:PTPv2 0\r\n"
            "a=sync-time:0\r\n"
            "a=ts-refclk:ptp=IEEE1588-2008:00-0B-72-FF-FE-07-DC-FC:0\r\n"
            "a=mediaclk:direct=0\r\n";

        auto result = rav::sdp::parse_session_description(k_mic8_sdp);
        REQUIRE(result);

        SECTION("Test origin") {
            const auto& origin = result->origin;
            REQUIRE(origin.username == "-");
            REQUIRE(origin.session_id == "1731086923289383");
            REQUIRE(origin.session_version == 0);
            REQUIRE(origin.network_type == rav::sdp::NetwType::internet);
            REQUIRE(origin.address_type == rav::sdp::AddrType::ipv4);
            REQUIRE(origin.unicast_address == "192.168.4.8");
        }

        SECTION("Test session name") {
            REQUIRE(result->session_name == "MADI-1");
        }

        SECTION("Test time") {
            auto time = result->time_active;
            REQUIRE(time.start_time == 0);
            REQUIRE(time.stop_time == 0);
        }

        SECTION("Test clock-domain") {
            auto clock_domain = result->ravenna_clock_domain.value();
            REQUIRE(clock_domain.source == rav::sdp::RavennaClockDomain::SyncSource::ptp_v2);
            REQUIRE(clock_domain.domain == 0);
        }

        SECTION("Test sync-time") {
            REQUIRE(result->ravenna_sync_time.value() == 0);
        }

        SECTION("Test refclk on session") {
            const auto& refclk = result->reference_clock;
            REQUIRE(refclk.has_value());
            REQUIRE(refclk->source_ == rav::sdp::ReferenceClock::ClockSource::ptp);
            REQUIRE(refclk->ptp_version_ == rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008);
            REQUIRE(refclk->gmid_ == "00-0B-72-FF-FE-07-DC-FC");
            REQUIRE(refclk->domain_ == 0);
        }

        SECTION("Test mediaclk attribute") {
            auto media_clock = result->media_clock.value();
            REQUIRE(media_clock.mode == rav::sdp::MediaClockSource::ClockMode::direct);
            REQUIRE(media_clock.offset.value() == 0);
            REQUIRE_FALSE(media_clock.rate.has_value());
        }

        SECTION("Test group") {
            REQUIRE(result->group);
            REQUIRE(result->group->type == rav::sdp::Group::Type::dup);
            REQUIRE(result->group->tags.size() == 2);
            REQUIRE(result->group->tags[0] == "primary");
            REQUIRE(result->group->tags[1] == "secondary");
        }

        SECTION("Test media") {
            const auto& descriptions = result->media_descriptions;
            REQUIRE(descriptions.size() == 2);

            SECTION("Primary") {
                const auto& media = descriptions[0];
                REQUIRE(media.media_type == "audio");
                REQUIRE(media.port == 5004);
                REQUIRE(media.number_of_ports == 1);
                REQUIRE(media.protocol == "RTP/AVP");
                REQUIRE(media.formats.size() == 1);

                auto format = media.formats[0];
                REQUIRE(format.payload_type == 98);
                REQUIRE(format.encoding_name == "L24");
                REQUIRE(format.clock_rate == 48000);
                REQUIRE(format.num_channels == 64);
                REQUIRE(media.connection_infos.size() == 1);

                const auto& conn = media.connection_infos.back();
                REQUIRE(conn.network_type == rav::sdp::NetwType::internet);
                REQUIRE(conn.address_type == rav::sdp::AddrType::ipv4);
                REQUIRE(conn.address == "239.3.8.1");
                REQUIRE(conn.ttl.has_value() == true);
                REQUIRE(*conn.ttl == 31);
                REQUIRE(static_cast<int64_t>(media.ptime.value() * 100.f) == 12);

                SECTION("Test refclk on media") {
                    const auto& refclk = media.reference_clock;
                    REQUIRE(refclk.has_value());
                    REQUIRE(refclk->source_ == rav::sdp::ReferenceClock::ClockSource::ptp);
                    REQUIRE(refclk->ptp_version_ == rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008);
                    REQUIRE(refclk->gmid_ == "00-0B-72-FF-FE-07-DC-FC");
                    REQUIRE(refclk->domain_ == 0);
                }

                SECTION("Test sync-time") {
                    REQUIRE(media.ravenna_sync_time.value() == 0);
                }

                SECTION("Test mediaclk on media") {
                    const auto& media_clock = media.media_clock.value();
                    REQUIRE(media_clock.mode == rav::sdp::MediaClockSource::ClockMode::direct);
                    REQUIRE(media_clock.offset.value() == 0);
                    REQUIRE_FALSE(media_clock.rate.has_value());
                }

                SECTION("Test source-filter on media") {
                    const auto& filters = media.source_filters;
                    REQUIRE(filters.size() == 1);
                    const auto& filter = filters[0];
                    REQUIRE(filter.mode == rav::sdp::FilterMode::include);
                    REQUIRE(filter.net_type == rav::sdp::NetwType::internet);
                    REQUIRE(filter.addr_type == rav::sdp::AddrType::ipv4);
                    REQUIRE(filter.dest_address == "239.3.8.1");
                    REQUIRE(filter.src_list.size() == 1);
                    REQUIRE(filter.src_list[0] == "192.168.16.52");
                }

                SECTION("Framecount") {
                    REQUIRE(media.ravenna_framecount == 6);
                }

                SECTION("Mid") {
                    auto mid = media.mid;
                    REQUIRE(mid.has_value());
                    REQUIRE(*mid == "primary");
                }
            }

            SECTION("Secondary") {
                const auto& media = descriptions[1];
                REQUIRE(media.media_type == "audio");
                REQUIRE(media.port == 5004);
                REQUIRE(media.number_of_ports == 1);
                REQUIRE(media.protocol == "RTP/AVP");
                REQUIRE(media.formats.size() == 1);

                auto format = media.formats[0];
                REQUIRE(format.payload_type == 98);
                REQUIRE(format.encoding_name == "L24");
                REQUIRE(format.clock_rate == 48000);
                REQUIRE(format.num_channels == 64);
                REQUIRE(media.connection_infos.size() == 1);

                const auto& conn = media.connection_infos.back();
                REQUIRE(conn.network_type == rav::sdp::NetwType::internet);
                REQUIRE(conn.address_type == rav::sdp::AddrType::ipv4);
                REQUIRE(conn.address == "239.4.8.2");
                REQUIRE(conn.ttl.has_value() == true);
                REQUIRE(*conn.ttl == 31);
                REQUIRE(static_cast<int64_t>(media.ptime.value() * 100.f) == 12);

                SECTION("Test refclk on media") {
                    const auto& refclk = media.reference_clock;
                    REQUIRE(refclk.has_value());
                    REQUIRE(refclk->source_ == rav::sdp::ReferenceClock::ClockSource::ptp);
                    REQUIRE(refclk->ptp_version_ == rav::sdp::ReferenceClock::PtpVersion::IEEE_1588_2008);
                    REQUIRE(refclk->gmid_ == "00-0B-72-FF-FE-07-DC-FC");
                    REQUIRE(refclk->domain_ == 0);
                }

                SECTION("Test sync-time") {
                    REQUIRE(media.ravenna_sync_time.value() == 0);
                }

                SECTION("Test mediaclk on media") {
                    const auto& media_clock = media.media_clock.value();
                    REQUIRE(media_clock.mode == rav::sdp::MediaClockSource::ClockMode::direct);
                    REQUIRE(media_clock.offset.value() == 0);
                    REQUIRE_FALSE(media_clock.rate.has_value());
                }

                SECTION("Test source-filter on media") {
                    const auto& filters = media.source_filters;
                    REQUIRE(filters.size() == 1);
                    const auto& filter = filters[0];
                    REQUIRE(filter.mode == rav::sdp::FilterMode::include);
                    REQUIRE(filter.net_type == rav::sdp::NetwType::internet);
                    REQUIRE(filter.addr_type == rav::sdp::AddrType::ipv4);
                    REQUIRE(filter.dest_address == "239.4.8.2");
                    REQUIRE(filter.src_list.size() == 1);
                    REQUIRE(filter.src_list[0] == "192.168.4.8");
                }

                SECTION("Framecount") {
                    REQUIRE(media.ravenna_framecount == 6);
                }

                SECTION("Mid") {
                    auto mid = media.mid;
                    REQUIRE(mid.has_value());
                    REQUIRE(*mid == "secondary");
                }
            }
        }
    }
}
