/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravenna_receiver.test.hpp"
#include "ravenna_sender.test.hpp"
#include "ravennakit/ravenna/ravenna_node.hpp"
#include "../core/net/interfaces/network_interface_config.test.hpp"
#include "../nmos/nmos_node.test.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("RavennaNode") {
    rav::AudioFormat audio_format;
    audio_format.encoding = rav::AudioEncoding::pcm_s24;
    audio_format.byte_order = rav::AudioFormat::ByteOrder::be;
    audio_format.num_channels = 2;
    audio_format.sample_rate = 48000;

    rav::RavennaReceiver::Configuration receiver1;
    receiver1.auto_update_sdp = true;
    receiver1.delay_frames = 480;
    receiver1.enabled = false;
    receiver1.session_name = "Receiver 1";
    receiver1.sdp =
        rav::sdp::SessionDescription::parse_new("v=0\r\no=- 13 0 IN IP4 192.168.15.52\r\ns=Anubis_610120_13\r\n")
            .value();

    rav::RavennaReceiver::Configuration receiver2;
    receiver2.auto_update_sdp = true;
    receiver2.delay_frames = 48;
    receiver2.enabled = false;
    receiver2.session_name = "Receiver 2";
    receiver2.sdp =
        rav::sdp::SessionDescription::parse_new("v=0\r\no=- 13 0 IN IP4 192.168.15.52\r\ns=Anubis_610120_13\r\n")
            .value();

    rav::RavennaSender::Destination primary;
    primary.enabled = true;
    primary.endpoint = boost::asio::ip::udp::endpoint(boost::asio::ip::make_address_v4("192.168.1.1"), 1234);
    primary.interface_by_rank = rav::Rank(0);

    rav::RavennaSender::Destination secondary;
    secondary.enabled = true;
    secondary.endpoint = boost::asio::ip::udp::endpoint(boost::asio::ip::make_address_v4("192.168.1.2"), 2345);
    secondary.interface_by_rank = rav::Rank(1);

    rav::RavennaSender::Configuration sender1;
    sender1.enabled = false;
    sender1.session_name = "Sender 1";
    sender1.audio_format = audio_format;
    sender1.packet_time = rav::aes67::PacketTime::ms_1();
    sender1.payload_type = 98;
    sender1.destinations.emplace_back(primary);
    sender1.destinations.emplace_back(secondary);

    rav::RavennaSender::Configuration sender2;
    sender2.enabled = false;
    sender2.session_name = "Sender 2";
    sender2.audio_format = audio_format;
    sender2.packet_time = rav::aes67::PacketTime::ms_4();
    sender2.payload_type = 99;
    sender2.destinations.emplace_back(primary);
    sender2.destinations.emplace_back(secondary);

    rav::nmos::Node::Configuration node_config;
    node_config.enabled = true;
    node_config.api_port = 8008;
    node_config.description = "Node description";
    node_config.label = "Node label";
    node_config.id = boost::uuids::random_generator()();
    node_config.operation_mode = rav::nmos::OperationMode::mdns_p2p;
    node_config.registry_address = "127.0.0.1";

    rav::RavennaConfig ravenna_config;
    ravenna_config.network_interfaces.set_interface(rav::Rank(0), "en0-not-valid");
    ravenna_config.network_interfaces.set_interface(rav::Rank(1), "en1-not-valid");

    rav::RavennaNode ravenna_node;
    ravenna_node.set_network_interface_config(ravenna_config.network_interfaces).get();
    const auto id1 = ravenna_node.create_receiver(receiver1).get().value();
    REQUIRE(id1.is_valid());
    const auto id2 = ravenna_node.create_receiver(receiver2).get().value();
    REQUIRE(id2.is_valid());
    const auto id3 = ravenna_node.create_sender(sender1).get().value();
    REQUIRE(id3.is_valid());
    const auto id4 = ravenna_node.create_sender(sender2).get().value();
    REQUIRE(id4.is_valid());
    REQUIRE(ravenna_node.set_nmos_configuration(node_config).get().has_value());

    SECTION("nlohmann json") {
        auto json = ravenna_node.to_json().get();
        INFO(json.dump());
        rav::test_network_interface_config_json(
            ravenna_config.network_interfaces, json.at("config").at("network_config")
        );
        rav::nmos::test_nmos_node_configuration_json(
            node_config, rav::nlohmann_to_boost_json(json.at("nmos_node").at("configuration"))
        );
        REQUIRE(
            boost::uuids::string_generator()(json.at("nmos_device_id").get<std::string>())
            == ravenna_node.get_nmos_device_id().get()
        );

        auto json_senders = json.at("senders");
        REQUIRE(json_senders.is_array());
        REQUIRE(json_senders.size() == 2);

        rav::test_ravenna_sender_configuration_json(sender1, json_senders.at(0).at("configuration"));
        rav::test_ravenna_sender_configuration_json(sender2, json_senders.at(1).at("configuration"));

        auto json_receivers = json.at("receivers");
        INFO(json_receivers.dump());
        REQUIRE(json_receivers.is_array());
        REQUIRE(json_receivers.size() == 2);

        rav::test_ravenna_receiver_configuration_json(receiver1, json_receivers.at(0).at("configuration"));
        rav::test_ravenna_receiver_configuration_json(receiver2, json_receivers.at(1).at("configuration"));
    }
}
