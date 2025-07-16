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
#include "ravennakit/core/util/todo.hpp"
#include "ravennakit/ravenna/ravenna_node.hpp"

#include <CLI/App.hpp>
#include <utility>

// This example is not complete and is not intended to be used as-is.

namespace examples {

struct RavennaNode final: rav::RavennaNode::Subscriber, rav::RavennaReceiver::Subscriber {
    explicit RavennaNode(const rav::NetworkInterfaceConfig& network_interface_config) {
        node.set_network_interface_config(network_interface_config).wait();
        node.subscribe(this).wait();
    }

    ~RavennaNode() override {
        node.unsubscribe(this).wait();
    }

    void ravenna_node_discovered(const rav::dnssd::ServiceDescription& desc) override {
        RAV_INFO("RAVENNA node discovered: {}", desc.to_string());
    }

    void ravenna_node_removed(const rav::dnssd::ServiceDescription& desc) override {
        RAV_INFO("RAVENNA node removed: {}", desc.to_string());
    }

    void ravenna_session_discovered(const rav::dnssd::ServiceDescription& desc) override {
        RAV_INFO("RAVENNA session discovered: {}", desc.to_string());
    }

    void ravenna_session_removed(const rav::dnssd::ServiceDescription& desc) override {
        RAV_INFO("RAVENNA session removed: {}", desc.to_string());
    }

    void ravenna_sender_added(const rav::RavennaSender& sender) override {
        RAV_INFO("RAVENNA sender added for: {}", sender.get_configuration().session_name);
    }

    void ravenna_sender_removed(const rav::Id sender_id) override {
        RAV_INFO("RAVENNA sender removed: {}", sender_id.value());
    }

    void ravenna_receiver_added(const rav::RavennaReceiver& receiver) override {
        RAV_INFO("RAVENNA receiver added for: {}", receiver.get_configuration().session_name);
    }

    void ravenna_receiver_removed(const rav::Id receiver_id) override {
        RAV_INFO("RAVENNA receiver removed: {}", receiver_id.value());
    }

    void ravenna_receiver_configuration_updated(
        const rav::RavennaReceiver& receiver, const rav::RavennaReceiver::Configuration& configuration
    ) override {
        RAV_INFO("RAVENNA configuration updated for receiver: {}", receiver.get_id().value());
        std::ignore = configuration;
    }

    void ravenna_receiver_parameters_updated(const rav::rtp::Receiver3::ReaderParameters& parameters) override {
        RAV_INFO("RAVENNA parameters updated: {}", parameters.audio_format.to_string());
    }

    rav::RavennaNode node;
};

}  // namespace examples

/**
 * This example demonstrates the use of the RavennaNode class to implement a virtual RAVENNA node. This is the easiest
 * and recommended way of sending and receiving RAVENNA streams.
 * Warning! This example is not complete and is not intended to be used as-is.
 */
int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    CLI::App app {"RAVENNA Receiver example"};
    argv = app.ensure_utf8(argv);

    std::vector<std::string> stream_names;
    app.add_option("stream_names", stream_names, "The name of the streams to receive (at least one)")->required();

    std::string interface_search_string;
    app.add_option(
        "--primary-interface", interface_search_string,
        "The primary interface address. The value can be the identifier, display name, description, MAC or an ip address."
    );

    CLI11_PARSE(app, argc, argv);

    const auto list = rav::NetworkInterfaceList::get_system_interfaces();
    const auto* primary_interface = list.find_by_string(interface_search_string);

    if (!primary_interface) {
        RAV_ERROR("Failed to find primary interface for: {}", interface_search_string);
        return 1;
    }

    rav::NetworkInterfaceConfig interface_config;
    interface_config.set_interface(rav::Rank::primary(), primary_interface->get_identifier());

    examples::RavennaNode node_example(interface_config);

    for (const auto& session : stream_names) {
        rav::RavennaReceiver::Configuration config;
        config.session_name = session;
        config.enabled = true;
        config.delay_frames = 480;  // 10ms at 48KHz
        node_example.node.create_receiver(config).wait();
    }

    fmt::println("Press return key to stop...");
    std::cin.get();

    return 0;
}
