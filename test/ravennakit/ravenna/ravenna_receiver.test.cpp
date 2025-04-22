/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_receiver.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("RavennaReceiver | Create audio receiver parameters") {
    SECTION("Create audio receiver parameters from Anubis SDP") {
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

        auto result = rav::sdp::SessionDescription::parse_new(k_anubis_sdp);
        REQUIRE(result.is_ok());

        auto parameters = rav::RavennaReceiver::create_audio_receiver_parameters(result.get_ok());
        REQUIRE(parameters);

        REQUIRE(parameters->audio_format.is_valid());
        REQUIRE(parameters->audio_format.encoding == rav::AudioEncoding::pcm_s16);
        REQUIRE(parameters->audio_format.sample_rate == 48000);
        REQUIRE(parameters->audio_format.num_channels == 2);
        REQUIRE(parameters->audio_format.byte_order == rav::AudioFormat::ByteOrder::be);
        REQUIRE(parameters->audio_format.ordering == rav::AudioFormat::ChannelOrdering::interleaved);

        REQUIRE(parameters->streams.size() == 1);
        REQUIRE(parameters->streams[0].session.connection_address == asio::ip::make_address_v4("239.1.15.52"));
        REQUIRE(parameters->streams[0].session.rtp_port == 5004);
        REQUIRE(parameters->streams[0].session.rtcp_port == 5005);
        REQUIRE(parameters->streams[0].packet_time_frames == 48);
        REQUIRE(parameters->streams[0].rank == rav::Rank(0));
    }

    SECTION("Create audio receiver parameters from Lawo SDP") {
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

        auto result = rav::sdp::SessionDescription::parse_new(k_mic8_sdp);
        REQUIRE(result.is_ok());

        auto parameters = rav::RavennaReceiver::create_audio_receiver_parameters(result.get_ok());
        REQUIRE(parameters);

        REQUIRE(parameters->audio_format.is_valid());
        REQUIRE(parameters->audio_format.encoding == rav::AudioEncoding::pcm_s24);
        REQUIRE(parameters->audio_format.sample_rate == 48000);
        REQUIRE(parameters->audio_format.num_channels == 64);
        REQUIRE(parameters->audio_format.byte_order == rav::AudioFormat::ByteOrder::be);
        REQUIRE(parameters->audio_format.ordering == rav::AudioFormat::ChannelOrdering::interleaved);

        REQUIRE(parameters->streams.size() == 2);
        REQUIRE(parameters->streams[0].session.connection_address == asio::ip::make_address_v4("239.3.8.1"));
        REQUIRE(parameters->streams[0].session.rtp_port == 5004);
        REQUIRE(parameters->streams[0].session.rtcp_port == 5005);
        REQUIRE(parameters->streams[0].packet_time_frames == 6);
        REQUIRE(parameters->streams[0].rank == rav::Rank(0));

        REQUIRE(parameters->streams[1].session.connection_address == asio::ip::make_address_v4("239.4.8.2"));
        REQUIRE(parameters->streams[1].session.rtp_port == 5004);
        REQUIRE(parameters->streams[1].session.rtcp_port == 5005);
        REQUIRE(parameters->streams[1].packet_time_frames == 6);
        REQUIRE(parameters->streams[1].rank == rav::Rank(1));
    }
}
