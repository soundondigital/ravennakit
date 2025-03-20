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
#include "ravennakit/rtsp/rtsp_client.hpp"

#include <CLI/App.hpp>

/**
 * This example shows how to create a RTSP client.
 */

int main(int const argc, char* argv[]) {
    rav::set_log_level_from_env();
    rav::do_system_checks();

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

    rav::rtsp::Client client(io_context);

    client.on<rav::rtsp::Connection::ConnectEvent>([path, &client](const rav::rtsp::Connection::ConnectEvent&) {
        RAV_INFO("Connected, send DESCRIBE request");
        client.async_describe(path);
    });

    client.on<rav::rtsp::Connection::RequestEvent>([](const rav::rtsp::Connection::RequestEvent& event) {
        RAV_INFO("{}\n{}", event.rtsp_request.to_debug_string(true), rav::string_replace(event.rtsp_request.data, "\r\n", "\n"));
    });

    client.on<rav::rtsp::Connection::ResponseEvent>([](const rav::rtsp::Connection::ResponseEvent& event) {
        RAV_INFO(
            "{}\n{}", event.rtsp_response.to_debug_string(true), rav::string_replace(event.rtsp_response.data, "\r\n", "\n")
        );
    });

    client.async_connect(host, port);

    io_context.run();
}
