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

    std::string stream_name;
    app.add_option("stream_name", stream_name, "The name of the stream to receive")->required();

    CLI11_PARSE(app, argc, argv);

    fmt::println("RAVENNA Receiver example. Receive from stream: {}", stream_name);

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
    rav::ravenna_sink sink1(rtsp_client, "Anubis Combo LR");
    rav::ravenna_sink sink2(rtsp_client, "Anubis_610120_16");

    io_context.run();
    node_browser.reset();
    cin_thread.join();
}

