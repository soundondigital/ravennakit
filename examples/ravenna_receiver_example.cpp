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
#if RAV_ENABLE_SPDLOG
    spdlog::set_level(spdlog::level::trace);
#endif

    rav::system::do_system_checks();

    CLI::App app {"RAVENNA Receiver example"};
    argv = app.ensure_utf8(argv);

    std::string stream_name;
    app.add_option("stream_name", stream_name, "The name of the stream to receive")->required();

    CLI11_PARSE(app, argc, argv);

    fmt::print("RAVENNA Receiver example. Receive from stream: {}\n", stream_name);

    asio::io_context io_context;

    auto node_browser = rav::dnssd::dnssd_browser::create(io_context);
    if (node_browser){
        node_browser->browse_for("_rtsp._tcp,_ravenna");

        rav::dnssd::dnssd_browser::subscriber subscriber{};
        subscriber->on<rav::dnssd::dnssd_browser::service_resolved>([](const auto& event) {
            RAV_INFO("RAVENNA Node resolved: {}", event.description.description());
        });
        node_browser->subscribe(subscriber);

        rav::ravenna_rtsp_client rtsp_client(io_context, *node_browser);
        // rav::ravenna_sink sink1(rtsp_client, "sink1");
        // rav::ravenna_sink sink2(rtsp_client, "sink2");
    }

    io_context.run();
}
