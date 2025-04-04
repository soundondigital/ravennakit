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

namespace examples {

struct ravenna_node final: rav::RavennaNode::Subscriber, rav::rtp::StreamReceiver::Subscriber {
    explicit ravenna_node(asio::ip::address_v4 interface_addr) : node(std::move(interface_addr)) {
        node.subscribe(this).wait();
    }

    ~ravenna_node() override {
        node.unsubscribe(this).wait();
    }

    void ravenna_node_discovered(const rav::dnssd::Browser::ServiceResolved& event) override {
        RAV_INFO("RAVENNA node discovered: {}", event.description.to_string());
    }

    void ravenna_node_removed(const rav::dnssd::Browser::ServiceRemoved& event) override {
        RAV_INFO("RAVENNA node removed: {}", event.description.to_string());
    }

    void ravenna_session_discovered(const rav::dnssd::Browser::ServiceResolved& event) override {
        RAV_INFO("RAVENNA session discovered: {}", event.description.to_string());
    }

    void ravenna_session_removed(const rav::dnssd::Browser::ServiceRemoved& event) override {
        RAV_INFO("RAVENNA session removed: {}", event.description.to_string());
    }

    void ravenna_receiver_added(const rav::RavennaReceiver& receiver) override {
        RAV_INFO("RAVENNA receiver added for: {}", receiver.get_session_name());
    }

    void on_rtp_stream_receiver_updated(const rav::rtp::StreamReceiver::StreamUpdatedEvent& event) override {
        RAV_INFO("Stream updated: {}", event.to_string());
    }

    rav::RavennaNode node;
};

}  // namespace examples

int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    CLI::App app {"RAVENNA Receiver example"};
    argv = app.ensure_utf8(argv);

    std::vector<std::string> stream_names;
    app.add_option("stream_names", stream_names, "The name of the streams to receive (at least one)")->required();

    std::string interface_address = "0.0.0.0";
    app.add_option("--interface-addr", interface_address, "The interface address");

    CLI11_PARSE(app, argc, argv);

    examples::ravenna_node node_example(asio::ip::make_address_v4(interface_address));

    for (auto& session : stream_names) {
        node_example.node.create_receiver(session).wait();
    }

    fmt::println("Press return key to stop...");
    std::cin.get();

    return 0;
}
