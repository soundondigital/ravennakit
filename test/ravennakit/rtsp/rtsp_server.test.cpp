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
#include "ravennakit/rtsp/rtsp_server.hpp"

#include <catch2/catch_all.hpp>

namespace {
constexpr int k_num_threads = 8;
}

TEST_CASE("rtsp_server", "[rtsp_server]") {
    SECTION("Port") {
        rav::io_context_runner runner(k_num_threads);

        SECTION("Any port") {
            const rav::rtsp_server server(runner.io_context(), asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 0));
            REQUIRE(server.port() != 0);
        }

        SECTION("Specific port") {
            const rav::rtsp_server server(runner.io_context(), asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 555));
            REQUIRE(server.port() == 555);
        }
    }

    SECTION("Create and destroy") {
        constexpr int k_num_threads = 8;
        rav::io_context_runner runner(k_num_threads);

        for (int i = 0; i < 10; i++) {
            rav::rtsp_server server(runner.io_context(), asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 0));
        }

        runner.wait_for_completion();
    }
}
