/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/net/http/http_server.hpp"
#include "ravennakit/core/net/http/http_client.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::HttpServer") {
    SECTION("Simple GET request") {
        boost::asio::io_context io_context;
        rav::HttpServer server(io_context);
        const auto start_result = server.start("127.0.0.1", 0);
        REQUIRE(!start_result.has_error());
        const auto endpoint = server.get_local_endpoint();
        REQUIRE(endpoint.port() != 0);

        server.get(
            "/",
            [](const rav::HttpServer::Request&, rav::HttpServer::Response& response, rav::PathMatcher::Parameters&) {
                response.result(boost::beast::http::status::ok);
                response.body() = "Hello, World!";
                response.prepare_payload();
            }
        );

        rav::HttpClient client(io_context, endpoint);
        client.get_async("/", [](auto response) {
            REQUIRE(response.has_value());
            REQUIRE(response->result() == boost::beast::http::status::ok);
        });

        client.get_async("/non-existent", [&io_context](auto response) {
            fmt::println("Response: {}", response.value());
            REQUIRE(response.has_value());
            REQUIRE(response->result() == boost::beast::http::status::not_found);
            // server.stop(); FIXME: We should do this, but there is a 5-second timeout somehow.
            io_context.stop();
        });

        io_context.run();
    }

    SECTION("Catch all handler") {
        boost::asio::io_context io_context;
        rav::HttpServer server(io_context);
        const auto start_result = server.start("127.0.0.1", 0);
        REQUIRE(!start_result.has_error());
        const auto endpoint = server.get_local_endpoint();
        REQUIRE(endpoint.port() != 0);

        server.get(
            "/test",
            [](const rav::HttpServer::Request&, rav::HttpServer::Response& response, rav::PathMatcher::Parameters&) {
                response.result(boost::beast::http::status::ok);
                response.body() = "/test";
                response.prepare_payload();
            }
        );

        // The order of the handlers matters. The catch-all handler should be last.
        server.get(
            "**",
            [](const rav::HttpServer::Request&, rav::HttpServer::Response& response, rav::PathMatcher::Parameters&) {
                response.result(boost::beast::http::status::ok);
                response.body() = "**";
                response.prepare_payload();
            }
        );

        rav::HttpClient client(io_context, endpoint);
        client.get_async("/", [](auto response) {
            REQUIRE(response.has_value());
            REQUIRE(response->result() == boost::beast::http::status::ok);
            REQUIRE(response->body() == "**");
        });

        client.get_async("/test", [](auto response) {
            REQUIRE(response.has_value());
            REQUIRE(response->result() == boost::beast::http::status::ok);
            REQUIRE(response->body() == "/test");
        });

        client.get_async("/some/deep/path", [](auto response) {
            REQUIRE(response.has_value());
            REQUIRE(response->result() == boost::beast::http::status::ok);
            REQUIRE(response->body() == "**");
        });

        client.get_async("/non-existent", [&io_context](auto response) {
            REQUIRE(response.has_value());
            REQUIRE(response->result() == boost::beast::http::status::ok);
            REQUIRE(response->body() == "**");
            // server.stop(); FIXME: We should do this, but there is a 5-second timeout somehow.
            io_context.stop();
        });

        io_context.run();
    }
}
