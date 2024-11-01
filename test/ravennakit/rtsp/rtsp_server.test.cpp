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
#include "ravennakit/rtsp/rtsp_client.hpp"
#include "ravennakit/rtsp/rtsp_server.hpp"

#include <catch2/catch_all.hpp>
#include <thread>

TEST_CASE("rtsp_server", "[rtsp_server]") {
    SECTION("Port") {
        asio::io_context io_context;

        std::thread thread([&] {
            io_context.run();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        SECTION("Any port") {
            rav::rtsp_server server(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 0));
            REQUIRE(server.port() != 0);
            server.close();
        }

        SECTION("Specific port") {
            rav::rtsp_server server(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 555));
            REQUIRE(server.port() == 555);
            server.close();
        }

        thread.join();
    }
}

TEST_CASE("rtsp_server | DESCRIBE", "[rtsp_server]") {
    const std::string test_data = "test data";
    asio::io_context io_context;
    rav::rtsp_server server(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 0));
    server.on<rav::rtsp_server::connection_event>([](const rav::rtsp_server::connection_event&) {
        RAV_TRACE("A new client connected");
    });
    server.on<rav::rtsp_server::request_event>([=](const rav::rtsp_server::request_event& event) {
        RAV_TRACE("{}", event.request.to_debug_string(true));
        event.connection.async_send_response(rav::rtsp_response(200, "OK", test_data));
    });
    server.on<rav::rtsp_server::response_event>([](const rav::rtsp_server::response_event& event) {
        RAV_TRACE("{}", event.response.to_debug_string(true));
    });

    const auto port = server.port();
    REQUIRE(port != 0);

    rav::rtsp_client client(io_context);
    client.on<rav::rtsp_connect_event>([](const rav::rtsp_connect_event& event) {
        RAV_TRACE("Connected, send DESCRIBE request");
        event.client.async_describe("/");
    });
    client.on<rav::rtsp_request>([](const rav::rtsp_request& request) {
        RAV_INFO("{}", request.to_debug_string(true));
    });
    client.on<rav::rtsp_response>([](const rav::rtsp_response& response) {
        RAV_INFO("{}", response.to_debug_string(true));
    });
    client.async_connect("::1", port);

    // io_context.run();
}
