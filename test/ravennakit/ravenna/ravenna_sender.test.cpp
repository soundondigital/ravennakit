/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_sender.hpp"

#include "../aes67/aes67_packet_time.test.hpp"
#include "../core/audio/audio_format.test.hpp"
#include "ravenna_sender.test.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::RavennaSender") {
    std::vector<rav::RavennaSender::Destination> destinations;
    destinations.push_back(
        {rav::Rank(0), boost::asio::ip::udp::endpoint(boost::asio::ip::make_address("239.0.0.1"), 5005), true}
    );
    destinations.push_back(
        {rav::Rank(1), boost::asio::ip::udp::endpoint(boost::asio::ip::make_address("239.0.0.2"), 5006), false}
    );

    for (auto& destination : destinations) {
        rav::test_ravenna_sender_destination_json(destination, boost::json::value_from(destination));
    }

    rav::AudioFormat audio_format;
    audio_format.byte_order = rav::AudioFormat::ByteOrder::be;
    audio_format.encoding = rav::AudioEncoding::pcm_s16;
    audio_format.num_channels = 2;
    audio_format.sample_rate = 44100;
    audio_format.ordering = rav::AudioFormat::ChannelOrdering::interleaved;

    test_audio_format_json(audio_format, boost::json::value_from(audio_format));

    rav::RavennaSender::Configuration config;
    config.enabled = false;
    config.audio_format = audio_format;
    config.packet_time = rav::aes67::PacketTime::ms_1();
    config.payload_type = 98;
    config.session_name = "Session name";
    config.ttl = 15;
    config.destinations = destinations;

    test_ravenna_sender_configuration_json(config, boost::json::value_from(config));

    boost::asio::io_context io_context;
    const auto advertiser = rav::dnssd::Advertiser::create(io_context);
    rav::rtsp::Server rtsp_server(io_context, "127.0.0.1", 0);
    rav::ptp::Instance ptp_instance(io_context);
    rav::rtp::AudioSender rtp_audio_sender(io_context);
    rav::RavennaSender sender(rtp_audio_sender, advertiser.get(), rtsp_server, ptp_instance, rav::Id {1}, 1, {});
    REQUIRE(sender.set_configuration(config).has_value());
    const auto sender_json = sender.to_boost_json();
    rav::test_ravenna_sender_json(sender, sender_json);
    rav::test_ravenna_sender_json(sender, sender.to_boost_json());

    rav::RavennaSender sender2(rtp_audio_sender, advertiser.get(), rtsp_server, ptp_instance, rav::Id {2}, 2, {});
    REQUIRE(sender2.restore_from_json(sender_json));
    rav::test_ravenna_sender_json(sender2, sender_json);
}

void rav::test_ravenna_sender_json(const RavennaSender& sender, const boost::json::value& json) {
    REQUIRE(json.is_object());
    REQUIRE(json.at("session_id") == sender.get_session_id());
    REQUIRE(json.at("nmos_sender_uuid").as_string() == boost::uuids::to_string(sender.get_nmos_sender().id));
    REQUIRE(json.at("nmos_source_uuid").as_string() == boost::uuids::to_string(sender.get_nmos_source().id));
    REQUIRE(json.at("nmos_flow_uuid").as_string() == boost::uuids::to_string(sender.get_nmos_flow().id));
    test_ravenna_sender_configuration_json(sender.get_configuration(), json.at("configuration"));
}

void rav::test_ravenna_sender_destination_json(
    const RavennaSender::Destination& destination, const boost::json::value& json
) {
    REQUIRE(json.at("enabled") == destination.enabled);
    REQUIRE(json.at("address").as_string() == destination.endpoint.address().to_string());
    REQUIRE(json.at("port") == destination.endpoint.port());
    REQUIRE(json.at("interface_by_rank") == destination.interface_by_rank.value());
}

void rav::test_ravenna_sender_destinations_json(
    const std::vector<RavennaSender::Destination>& destinations, const boost::json::value& json
) {
    REQUIRE(json.is_array());
    REQUIRE(json.as_array().size() == destinations.size());

    for (size_t i = 0; i < json.as_array().size(); i++) {
        test_ravenna_sender_destination_json(destinations.at(i), json.as_array().at(i));
    }
}

void rav::test_ravenna_sender_configuration_json(
    const RavennaSender::Configuration& config, const boost::json::value& json
) {
    REQUIRE(json.at("session_name").as_string() == config.session_name);
    REQUIRE(json.at("ttl") == config.ttl);
    REQUIRE(json.at("payload_type") == config.payload_type);
    REQUIRE(json.at("enabled") == config.enabled);
    test_ravenna_sender_destinations_json(config.destinations, json.at("destinations"));
    test_audio_format_json(config.audio_format, json.at("audio_format"));
    test_packet_time_json(config.packet_time, json.at("packet_time"));
}
