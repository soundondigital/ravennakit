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
#include "ravennakit/core/net/http/http_server.hpp"

int main() {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    boost::asio::io_context io_context;

    // Create a server instance
    rav::HttpServer server(io_context);

    server.get("/", [](const rav::HttpServer::Request&, rav::HttpServer::Response& response) {
        response.result(boost::beast::http::status::ok);
        response.set(boost::beast::http::field::content_type, "text/plain");
        response.body() = "Hello, World!";
        response.prepare_payload();
    });

    server.get("/shutdown", [&io_context, &server](const rav::HttpServer::Request&, rav::HttpServer::Response& response) {
        response.result(boost::beast::http::status::ok);
        response.set(boost::beast::http::field::content_type, "text/plain");
        response.body() = "Shutting down server...";
        response.prepare_payload();

        boost::asio::post(io_context, [&server] {
            server.stop();
        });
    });

    // Start the server
    const auto result = server.start("127.0.0.1", 8080);
    if (result.has_error()) {
        RAV_ERROR("Error starting server: {}", result.error().message());
        return 1;
    }

    RAV_INFO("Server started at http://{}", server.get_address_string());
    RAV_INFO("Visit http://{}/shutdown to stop the server", server.get_address_string());

    // Run the io_context to start accepting connections
    io_context.run();

    return 0;
}
