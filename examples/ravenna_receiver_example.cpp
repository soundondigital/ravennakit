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
#include "ravennakit/ravenna/ravenna_browser.hpp"

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
    rav::ravenna_browser browser(io_context);

    io_context.run();
}
