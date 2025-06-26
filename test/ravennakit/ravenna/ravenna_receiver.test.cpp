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
#include "ravenna_receiver.test.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::RavennaReceiver") {
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

        auto result = rav::sdp::parse_session_description(k_anubis_sdp);
        REQUIRE(result);

        auto parameters = rav::RavennaReceiver::create_audio_receiver_parameters(*result);
        REQUIRE(parameters);

        REQUIRE(parameters->audio_format.is_valid());
        REQUIRE(parameters->audio_format.encoding == rav::AudioEncoding::pcm_s16);
        REQUIRE(parameters->audio_format.sample_rate == 48000);
        REQUIRE(parameters->audio_format.num_channels == 2);
        REQUIRE(parameters->audio_format.byte_order == rav::AudioFormat::ByteOrder::be);
        REQUIRE(parameters->audio_format.ordering == rav::AudioFormat::ChannelOrdering::interleaved);

        REQUIRE(parameters->streams.size() == 1);
        REQUIRE(parameters->streams[0].session.connection_address == boost::asio::ip::make_address_v4("239.1.15.52"));
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

        auto result = rav::sdp::parse_session_description(k_mic8_sdp);
        REQUIRE(result);

        auto parameters = rav::RavennaReceiver::create_audio_receiver_parameters(*result);
        REQUIRE(parameters);

        REQUIRE(parameters->audio_format.is_valid());
        REQUIRE(parameters->audio_format.encoding == rav::AudioEncoding::pcm_s24);
        REQUIRE(parameters->audio_format.sample_rate == 48000);
        REQUIRE(parameters->audio_format.num_channels == 64);
        REQUIRE(parameters->audio_format.byte_order == rav::AudioFormat::ByteOrder::be);
        REQUIRE(parameters->audio_format.ordering == rav::AudioFormat::ChannelOrdering::interleaved);

        REQUIRE(parameters->streams.size() == 2);
        REQUIRE(parameters->streams[0].session.connection_address == boost::asio::ip::make_address_v4("239.3.8.1"));
        REQUIRE(parameters->streams[0].session.rtp_port == 5004);
        REQUIRE(parameters->streams[0].session.rtcp_port == 5005);
        REQUIRE(parameters->streams[0].packet_time_frames == 6);
        REQUIRE(parameters->streams[0].rank == rav::Rank(0));

        REQUIRE(parameters->streams[1].session.connection_address == boost::asio::ip::make_address_v4("239.4.8.2"));
        REQUIRE(parameters->streams[1].session.rtp_port == 5004);
        REQUIRE(parameters->streams[1].session.rtcp_port == 5005);
        REQUIRE(parameters->streams[1].packet_time_frames == 6);
        REQUIRE(parameters->streams[1].rank == rav::Rank(1));
    }

    SECTION("To JSON") {
        rav::RavennaReceiver::Configuration config;
        config.session_name = "Session name";
        config.auto_update_sdp = true;
        config.enabled = false;
        config.delay_frames = 480;
        config.sdp =
            rav::sdp::parse_session_description("v=0\r\no=- 1731086923289383 0 IN IP4 192.168.4.8\r\n").value();

        rav::test_ravenna_receiver_configuration_json(config, boost::json::value_from(config));

#if !RAV_LINUX
        // On Linux there is no implementation for the dnssd browser which makes the next code error out. Until the
        // browser is implemented we'll keep the tests disabled.
        boost::asio::io_context io_context;
        rav::RavennaBrowser ravenna_browser(io_context);
        rav::RavennaRtspClient rtsp_client(io_context, ravenna_browser);
        rav::UdpReceiver udp_receiver(io_context);
        rav::rtp::Receiver rtp_receiver(udp_receiver);
        rav::RavennaReceiver receiver(io_context, rtsp_client, rtp_receiver, rav::Id {1});
        REQUIRE(receiver.set_configuration(config));
        rav::test_ravenna_receiver_json(receiver, receiver.to_boost_json());
#endif
    }

    SECTION("From JSON") {
        rav::RavennaReceiver::Configuration config;
        config.session_name = "Session name";
        config.auto_update_sdp = true;
        config.enabled = false;
        config.delay_frames = 480;
        config.sdp =
            rav::sdp::parse_session_description("v=0\r\no=- 1731086923289383 0 IN IP4 192.168.4.8\r\n").value();

        auto json = boost::json::value_from(config);
        auto restored = boost::json::value_to<rav::RavennaReceiver::Configuration>(json);
        rav::test_ravenna_receiver_configuration_json(restored, json);

#if !RAV_LINUX
        // On Linux there is no implementation for the dnssd browser which makes the next code error out. Until the
        // browser is implemented we'll keep the tests disabled.
        boost::asio::io_context io_context;
        rav::RavennaBrowser ravenna_browser(io_context);
        rav::RavennaRtspClient rtsp_client(io_context, ravenna_browser);
        rav::UdpReceiver udp_receiver(io_context);
        rav::rtp::Receiver rtp_receiver(udp_receiver);
        rav::RavennaReceiver receiver(io_context, rtsp_client, rtp_receiver, rav::Id {1});
        REQUIRE(receiver.set_configuration(config));
        const auto receiver_json = receiver.to_boost_json();
        REQUIRE(receiver.restore_from_json(receiver_json));
        rav::test_ravenna_receiver_json(receiver, receiver_json);
#endif
    }
}

void rav::test_ravenna_receiver_json(const RavennaReceiver& receiver, const boost::json::value& json) {
    REQUIRE(json.is_object());
    REQUIRE(json.at("nmos_receiver_uuid").as_string() == boost::uuids::to_string(receiver.get_nmos_receiver().id));
    test_ravenna_receiver_configuration_json(receiver.get_configuration(), json.at("configuration"));
}

void rav::test_ravenna_receiver_configuration_json(
    const RavennaReceiver::Configuration& config, const boost::json::value& json
) {
    REQUIRE(json.is_object());
    REQUIRE(json.at("session_name").as_string() == config.session_name);
    REQUIRE(json.at("auto_update_sdp") == config.auto_update_sdp);
    REQUIRE(json.at("enabled") == config.enabled);
    REQUIRE(json.at("delay_frames") == config.delay_frames);
    REQUIRE(json.at("sdp").as_string() == rav::sdp::to_string(config.sdp));
}
