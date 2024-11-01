/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/asio/io_context_runner.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/system.hpp"
#include "ravennakit/rtsp/rtsp_client.hpp"

#include <CLI/App.hpp>

int main(int const argc, char* argv[]) {
    rav::log::set_level_from_env();
    rav::system::do_system_checks();

    CLI::App app {"RTSP Client example"};
    argv = app.ensure_utf8(argv);

    std::string host;
    app.add_option("host", host, "The host to connect to")->required();

    std::string port;
    app.add_option("port", port, "The port to connect to")->required();

    std::string path;
    app.add_option("path", path, "The path of the stream (/by-id/13 or /by-name/stream%20name)")->required();

    CLI11_PARSE(app, argc, argv);

    asio::io_context io_context;

    rav::rtsp_client client(io_context);

    client.on<rav::rtsp_connect_event>([path, &client](const rav::rtsp_connect_event&) {
        RAV_INFO("Connected, send DESCRIBE request");
        client.async_describe(path);
    });

    client.on<rav::rtsp_request>([](const rav::rtsp_request& request) {
        RAV_INFO("{}\n{}", request.to_debug_string(true), rav::string_replace(request.data, "\r\n", "\n"));
    });

    client.on<rav::rtsp_response>([](const rav::rtsp_response& response) {
        RAV_INFO("{}\n{}", response.to_debug_string(true), rav::string_replace(response.data, "\r\n", "\n"));
    });

    client.async_connect(host, port);

    io_context.run();
}
