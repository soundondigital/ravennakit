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

struct ravenna_node_example final: rav::ravenna_node::subscriber, rav::rtp_stream_receiver::subscriber {
    explicit ravenna_node_example(const rav::rtp_receiver::configuration& config) : node(config) {
        node.subscribe(this).wait();
    }

    ~ravenna_node_example() override {
        node.unsubscribe(this).wait();
    }

    void ravenna_node_discovered(const rav::dnssd::dnssd_browser::service_resolved& event) override {
        RAV_INFO("RAVENNA node discovered: {}", event.description.to_string());
    }

    void ravenna_node_removed(const rav::dnssd::dnssd_browser::service_removed& event) override {
        RAV_INFO("RAVENNA node removed: {}", event.description.to_string());
    }

    void ravenna_session_discovered(const rav::dnssd::dnssd_browser::service_resolved& event) override {
        RAV_INFO("RAVENNA session discovered: {}", event.description.to_string());
    }

    void ravenna_session_removed(const rav::dnssd::dnssd_browser::service_removed& event) override {
        RAV_INFO("RAVENNA session removed: {}", event.description.to_string());
    }

    void ravenna_receiver_added(const rav::ravenna_receiver& receiver) override {
        RAV_INFO("RAVENNA receiver added for: {}", receiver.get_session_name());
    }

    void rtp_stream_receiver_updated(const rav::rtp_stream_receiver::stream_updated_event& event) override {
        RAV_INFO("Stream updated: {}", event.to_string());
    }

    rav::ravenna_node node;
};

int main(int const argc, char* argv[]) {
    rav::log::set_level_from_env();
    rav::system::do_system_checks();

    CLI::App app {"RAVENNA Receiver example"};
    argv = app.ensure_utf8(argv);

    std::vector<std::string> stream_names;
    app.add_option("stream_names", stream_names, "The name of the streams to receive (at least one)")->required();

    std::string interface_address = "0.0.0.0";
    app.add_option("--interface-addr", interface_address, "The interface address");

    CLI11_PARSE(app, argc, argv);

    rav::rtp_receiver::configuration config;
    config.interface_address = asio::ip::make_address(interface_address);

    ravenna_node_example node_example(config);

    for (auto& session : stream_names) {
        node_example.node.create_receiver(session).wait();
    }

    fmt::println("Press return key to stop...");
    std::cin.get();

    return 0;
}
