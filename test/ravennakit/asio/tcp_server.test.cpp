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
#include "ravennakit/asio/tcp_server.hpp"

#include <catch2/catch_all.hpp>
#include <asio/io_context.hpp>

#include <thread>

TEST_CASE("tcp_server", "[tcp_server]") {
    constexpr int k_num_threads = 8;
    rav::io_context_runner runner(k_num_threads);

    SECTION("Any port") {
        rav::tcp_server server(runner.io_context(), asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 0));
        REQUIRE(server.port() != 0);
    }

    SECTION("Specific port") {
        rav::tcp_server server(runner.io_context(), asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 555));
        REQUIRE(server.port() == 555);
    }
}

TEST_CASE("tcp_server | run multi threaded server", "[tcp_server]") {
    constexpr int k_num_threads = 8;
    rav::io_context_runner runner(k_num_threads);

    runner.start();

    for (int i = 0; i < 10; i++) {
        rav::tcp_server server(runner.io_context(), asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 0));
        server.stop();
    }

    rav::tcp_server server(runner.io_context(), asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 0));
    server.stop();
}
