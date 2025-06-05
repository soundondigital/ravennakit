/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/system.hpp"
#include "ravennakit/nmos/nmos_node.hpp"

#include <boost/asio/io_context.hpp>

int main() {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    boost::asio::io_context io_context;

    rav::nmos::Node::ConfigurationUpdate config;
    config.enabled = true;
    config.node_api_port = 8000;  // Set the port for the NMOS node API

    rav::nmos::Node node(io_context);
    node.update_configuration(config, true);

    static constexpr uint32_t k_num_devices = 2;
    static constexpr uint32_t k_num_sources_per_device = 2;
    static constexpr uint32_t k_num_senders_per_source = 2;
    static constexpr uint32_t k_num_receivers_per_device = 2;

    uint32_t device_count = 0;
    uint32_t source_count = 0;
    uint32_t sender_count = 0;
    uint32_t receiver_count = 0;
    uint32_t flow_count = 0;

    // Devices
    for (uint32_t i_device = 0; i_device < k_num_devices; ++i_device) {
        rav::nmos::Device::Control control;
        control.href = fmt::format("http://localhost:{}", i_device + 6000);
        control.type = fmt::format("urn:x-manufacturer:control:generic.{}", i_device + 1);
        control.authorization = i_device % 2 == 0;
        rav::nmos::Device device;
        device.id = boost::uuids::random_generator()();
        device.label = fmt::format("ravennakit/device/{}", device_count);
        device.description = fmt::format("RAVENNAKIT Device {}", device_count + 1);
        device.version = rav::nmos::Version {i_device + 1, (i_device + 1) * 1000};
        device.controls.push_back(control);
        std::ignore = node.add_or_update_device(device);

        // Sources
        for (uint32_t i_source = 0; i_source < k_num_sources_per_device; ++i_source) {
            rav::nmos::SourceAudio source;
            source.id = boost::uuids::random_generator()();
            source.label = fmt::format("ravennakit/device/{}/source/{}", device_count, source_count);
            source.description = fmt::format("RAVENNAKIT Device {} source {}", device_count + 1, source_count + 1);
            source.version = rav::nmos::Version {i_source + 1, (i_source + 1) * 1000};
            source.device_id = device.id;
            source.channels.push_back({"Channel 1"});
            std::ignore = node.add_or_update_source({source});

            // Flow
            for (uint32_t i_sender = 0; i_sender < k_num_senders_per_source; ++i_sender) {
                rav::nmos::FlowAudioRaw flow;
                flow.id = boost::uuids::random_generator()();
                flow.label = fmt::format("ravennakit/device/{}/flow/{}", device_count, flow_count);
                flow.description = fmt::format("RAVENNAKIT Device {} flow {}", device_count + 1, flow_count + 1);
                flow.version = rav::nmos::Version {i_sender + 1, (i_sender + 1) * 1000};
                flow.bit_depth = 24;
                flow.sample_rate = {48000, 1};
                flow.media_type = "audio/L24";
                flow.source_id = source.id;
                flow.device_id = device.id;
                std::ignore = node.add_or_update_flow({flow});

                // Sender
                rav::nmos::Sender sender;
                sender.id = boost::uuids::random_generator()();
                sender.label = fmt::format("ravennakit/device/{}/sender/{}", device_count, sender_count);
                sender.description =
                    fmt::format("RAVENNAKIT Device {} sender {}", device_count + 1, sender_count + 1);
                sender.version = rav::nmos::Version {i_sender + 1, (i_sender + 1) * 1000};
                sender.device_id = device.id;
                sender.transport = "urn:x-nmos:transport:rtp";
                sender.flow_id = flow.id;
                std::ignore = node.add_or_update_sender(sender);

                flow_count++;
                sender_count++;
            }

            source_count++;
        }

        // Receivers
        for (uint32_t i_receiver = 0; i_receiver < k_num_receivers_per_device; ++i_receiver) {
            rav::nmos::ReceiverAudio receiver;
            receiver.id = boost::uuids::random_generator()();
            receiver.label = fmt::format("ravennakit/device/{}/receiver/{}", device_count, receiver_count);
            receiver.description =
                fmt::format("RAVENNAKIT Device {} sender {}", device_count + 1, receiver_count + 1);
            receiver.version = rav::nmos::Version {i_receiver + 1, (i_receiver + 1) * 1000};
            receiver.device_id = device.id;
            receiver.transport = "urn:x-nmos:transport:rtp";
            receiver.caps.media_types = {"audio/L24", "audio/L20", "audio/L16", "audio/L8", "audio/PCM"};
            std::ignore = node.add_or_update_receiver({receiver});

            receiver_count++;
        }

        device_count++;
    }

    std::string url =
        fmt::format("http://{}:{}", node.get_local_endpoint().address().to_string(), node.get_local_endpoint().port());

    RAV_INFO("NMOS node started at {}", url);

    url += "/x-nmos";
    RAV_INFO("{}", url);

    url += "/node";
    RAV_INFO("{}", url);

    url += "/v1.3";
    RAV_INFO("{}", url);

    url += "/";
    RAV_INFO("{}", url);

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        RAV_INFO("Stopping NMOS node...");
        io_context.stop();
    });

    io_context.run();

    return 0;
}
