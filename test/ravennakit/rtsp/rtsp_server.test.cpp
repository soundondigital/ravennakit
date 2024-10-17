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

TEST_CASE("rtsp_server", "[rtsp_server]") {
    SECTION("Port") {
        asio::io_context io_context;
        std::vector<std::thread> threads;

        for (auto i = 0; i < 10; i++) {
            threads.emplace_back([&io_context] {
                io_context.run();
            });
        }

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

        for (auto& thread : threads) {
            thread.join();
        }
    }

    SECTION("Create and destroy using io_context_runner") {
        rav::io_context_runner runner(8);

        for (int i = 0; i < 10; i++) {
            rav::rtsp_server server(runner.io_context(), asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 0));
            server.close();
        }

        runner.join();
    }

    SECTION("Create and destroy using io_context") {
        asio::io_context io_context;
        std::vector<std::thread> threads;

        for (auto i = 0; i < 8; i++) {
            threads.emplace_back([&io_context] {
                io_context.run();
            });
        }

        for (int i = 0; i < 10; i++) {
            rav::rtsp_server server(io_context, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 0));
            server.close();
        }

        for (auto& thread : threads) {
            thread.join();
        }
    }
}
