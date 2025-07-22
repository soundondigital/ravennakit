/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/dnssd/dnssd_advertiser.hpp"
#include "ravennakit/ptp/ptp_instance.hpp"
#include "ravennakit/ravenna/ravenna_node.hpp"
#include "ravennakit/ravenna/ravenna_receiver.hpp"
#include "ravennakit/ravenna/ravenna_sender.hpp"
#include "ravennakit/rtp/detail/rtp_sender.hpp"
#include "ravennakit/rtsp/rtsp_server.hpp"

#include <CLI/App.hpp>

namespace {

constexpr uint32_t k_delay = 480;

/// Helper class which forwards virtual calls to rav::SafeFunction.
class RavennaReceiverSubscriber: public rav::RavennaReceiver::Subscriber {
  public:
    rav::SafeFunction<void(const rav::rtp::AudioReceiver::ReaderParameters& parameters)>
        on_ravenna_receiver_parameters_updated;

    void ravenna_receiver_parameters_updated(const rav::rtp::AudioReceiver::ReaderParameters& parameters) override {
        on_ravenna_receiver_parameters_updated(parameters);
    }
};

}  // namespace

/**
 * This example subscribes to a RAVENNA stream, reads the audio data, and writes it back to the network as a different
 * stream. The purpose of this example is to show (and test) how low the latency can be when using the RAVENNA API and
 * to demonstrate how the playback is sample-sync (although you'll have to measure than on a real device).
 */

int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    CLI::App app {"RAVENNA Loopback example"};
    argv = app.ensure_utf8(argv);

    std::string stream_name;
    app.add_option("stream_name", stream_name, "The name of the stream to loop back")->required();

    std::string interfaces;
    app.add_option(
        "--interfaces", interfaces,
        R"(The interfaces to use. Example 1: "en1,en2", example 2: "192.168.1.1,192.168.2.1")"
    );

    CLI11_PARSE(app, argc, argv);

    const auto network_config = rav::parse_network_interface_config_from_string(interfaces);
    if (!network_config) {
        RAV_ERROR("Failed to parse network interface config");
        return 1;
    }

    rav::RavennaNode ravenna_node;
    ravenna_node.set_network_interface_config(*network_config).wait();

    rav::RavennaReceiver::Configuration config;
    config.enabled = true;
    config.session_name = stream_name;
    // No need to set the delay, because RAVENNAKIT doesn't use this value

    auto receiver_id = ravenna_node.create_receiver(config).get().value();
    rav::Id sender_id;

    std::vector<uint8_t> buffer;

    RavennaReceiverSubscriber receiver_subscriber;
    receiver_subscriber.on_ravenna_receiver_parameters_updated =
        [&ravenna_node, &buffer, &stream_name, &sender_id](const rav::rtp::AudioReceiver::ReaderParameters& parameters) {
            if (parameters.streams.empty()) {
                RAV_WARNING("No streams available");
                return;
            }

            if (!parameters.is_valid()) {
                return;
            }

            buffer.resize(parameters.audio_format.bytes_per_frame() * parameters.streams.front().packet_time_frames);

            rav::RavennaSender::Configuration sender_config;
            sender_config.session_name = stream_name + "_loopback";
            sender_config.audio_format = parameters.audio_format;
            sender_config.enabled = true;
            sender_config.payload_type = 98;
            sender_config.ttl = 15;
            sender_config.packet_time = rav::aes67::PacketTime::ms_1();  // TODO: Update dynamically

            for (size_t i = 0; i < parameters.streams.size(); ++i) {
                if (parameters.streams[i].is_valid()) {
                    rav::RavennaSender::Destination destination {
                        rav::Rank(static_cast<uint8_t>(i)), boost::asio::ip::udp::endpoint {{}, 5004}, true
                    };
                    sender_config.destinations.emplace_back(destination);
                }
            }

            if (!sender_id.is_valid()) {
                const auto result = ravenna_node.create_sender(sender_config).get();
                if (!result) {
                    RAV_ERROR("Failed to create sender");
                    return;
                }
                sender_id = *result;
            } else {
                auto result = ravenna_node.update_sender_configuration(sender_id, sender_config).get();
                if (!result) {
                    RAV_ERROR("Failed to update sender: {}", result.error());
                }
            }
        };

    ravenna_node.subscribe_to_receiver(receiver_id, &receiver_subscriber).wait();

    rav::ptp::Instance::Subscriber ptp_subscriber;
    ravenna_node.subscribe_to_ptp_instance(&ptp_subscriber).wait();

    std::atomic keep_going = true;
    std::thread audio_thread([&]() mutable {
        while (keep_going.load(std::memory_order_relaxed)) {
            if (!ptp_subscriber.get_local_clock().is_calibrated()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            const auto ts = ravenna_node.read_data_realtime(receiver_id, buffer.data(), buffer.size(), {}, k_delay);
            if (!ts) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }

            rav::BufferView buffer_view(buffer);
            if (!ravenna_node.send_data_realtime(sender_id, buffer_view.const_view(), *ts + k_delay)) {
                RAV_ERROR("Failed to send data");
            }
        }
    });

    fmt::println("Press return key to stop...");
    std::string line;
    std::getline(std::cin, line);

    keep_going.store(false, std::memory_order_relaxed);
    audio_thread.join();

    return 0;
}
