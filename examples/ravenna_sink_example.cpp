/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/dnssd/bonjour/bonjour_browser.hpp"
#include "ravennakit/ravenna/ravenna_rtsp_client.hpp"
#include "ravennakit/ravenna/ravenna_sink.hpp"

#include <CLI/App.hpp>
#include <asio/io_context.hpp>

int main(int const argc, char* argv[]) {
    rav::log::set_level_from_env();
    rav::system::do_system_checks();

    CLI::App app {"RAVENNA Receiver example"};
    argv = app.ensure_utf8(argv);

    std::vector<std::string> stream_names;
    app.add_option("stream_name", stream_names, "The name of the stream to receive")->required();

    CLI11_PARSE(app, argc, argv);

    asio::io_context io_context;

    auto node_browser = rav::dnssd::dnssd_browser::create(io_context);

    if (node_browser == nullptr) {
        fmt::println("No dnssd browser available. Exiting.");
        exit(0);
    }

    node_browser->browse_for("_rtsp._tcp,_ravenna_session");

    std::thread cin_thread([&io_context] {
        fmt::println("Press return key to stop...");
        std::cin.get();
        io_context.stop();
    });

    rav::ravenna_rtsp_client rtsp_client(io_context, *node_browser);
    rav::rtp_receiver rtp_receiver(io_context);

    std::vector<std::unique_ptr<rav::ravenna_sink>> sinks;
    for (auto const& stream_name : stream_names) {
        sinks.emplace_back(std::make_unique<rav::ravenna_sink>(rtsp_client, rtp_receiver, stream_name))->start();
    }

    io_context.run();
    node_browser.reset();
    cin_thread.join();
}
