/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/rtsp_client.hpp"
#include "ravennakit/rtsp/rtsp_server.hpp"

#include <catch2/catch_all.hpp>
#include <thread>

TEST_CASE("rav::rtsp::Server") {
    SECTION("Port") {
        boost::asio::io_context io_context;

        std::thread thread([&] {
            io_context.run();
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        SECTION("Any port") {
            rav::rtsp::Server server(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 0));
            REQUIRE(server.port() != 0);
            server.stop();
        }

        SECTION("Specific port") {
            rav::rtsp::Server server(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), 5555));
            REQUIRE(server.port() == 5555);
            server.stop();
        }

        thread.join();
    }
}
