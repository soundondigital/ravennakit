/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/detail/rtp_receiver.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::rtp::Receiver") {
    boost::asio::io_context io_context;

    SECTION("Test bounds") {
        REQUIRE(rav::rtp::Receiver3::k_max_num_redundant_streams >= 1);
        REQUIRE(rav::rtp::Receiver3::k_max_num_rtp_sessions_per_stream >= 1);
        REQUIRE(
            rav::rtp::Receiver3::k_max_num_sessions
            == rav::rtp::Receiver3::k_max_num_redundant_streams * rav::rtp::Receiver3::k_max_num_rtp_sessions_per_stream
        );
    }

    SECTION("Initial state") {
        auto receiver = std::make_unique<rav::rtp::Receiver3>();

        // Sockets
        REQUIRE(decltype(receiver->sockets)::capacity() == rav::rtp::Receiver3::k_max_num_sessions);
        REQUIRE(receiver->sockets.empty());

        // Streams
        REQUIRE(decltype(receiver->streams)::capacity() == rav::rtp::Receiver3::k_max_num_redundant_streams);
        REQUIRE(receiver->streams.empty());
    }

    SECTION("Binding a UDP socket to the any address") {
        boost::asio::ip::udp::endpoint endpoint(boost::asio::ip::address_v4::any(), 0);
        boost::asio::ip::udp::socket socket(io_context, endpoint);
        REQUIRE(socket.is_open());
        REQUIRE(socket.local_endpoint().address() == endpoint.address());
        REQUIRE(socket.local_endpoint().port() != endpoint.port());
    }

    SECTION("Send and receive unicast UDP packets") {
        boost::asio::ip::udp::socket rx(io_context, {boost::asio::ip::address_v4::loopback(), 0});
        REQUIRE(rx.is_open());

        uint64_t base_value = 0x1234deadbeef5678;

        boost::asio::ip::udp::socket tx(io_context, {boost::asio::ip::address_v4::loopback(), 0});

        uint64_t num_packets = 200;

        for (uint64_t i = 0; i < num_packets; i++) {
            uint64_t send_value = base_value + i;
            tx.send_to(boost::asio::buffer(&send_value, sizeof(send_value)), rx.local_endpoint());
        }

        uint64_t received = 0;

        for (uint64_t i = 0; i < num_packets; i++) {
            rx.receive(boost::asio::buffer(&received, sizeof(received)));
            REQUIRE(received == base_value + i);
        }
    }

    SECTION("Send and receive to and from many multicast groups") {
        auto multicast_base_address = boost::asio::ip::make_address_v4("239.0.0.1");
        auto interface_address = boost::asio::ip::address_v4::loopback();
        constexpr uint32_t k_num_multicast_groups = 512;

#if RAV_WINDOWS
        boost::asio::ip::udp::socket rx(io_context, {interface_address, 0});
#else
        boost::asio::ip::udp::socket rx(io_context, {multicast_base_address, 0});
#endif

        for (uint32_t i = 0; i < k_num_multicast_groups; ++i) {
            boost::asio::ip::address_v4 addr(multicast_base_address.to_uint() + i);
            rx.set_option(boost::asio::ip::multicast::join_group(addr, interface_address));
        }

        boost::asio::ip::udp::socket tx(io_context, {interface_address, 0});
        tx.set_option(boost::asio::ip::multicast::outbound_interface(interface_address));

        std::atomic_bool keep_going = true;
        uint16_t port = rx.local_endpoint().port();

        std::thread tx_thead([&keep_going, multicast_base_address, &tx, port] {
            uint32_t i = 0;
            const boost::asio::ip::udp::endpoint endpoint(
                boost::asio::ip::address_v4(multicast_base_address.to_uint() + i), port
            );
            while (keep_going) {
                tx.send_to(boost::asio::buffer(&i, sizeof(i)), endpoint);
                if (++i >= k_num_multicast_groups) {
                    i = 0;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

        std::set<uint32_t> received;
        uint32_t receiver_buffer = 0;

        while (received.size() < k_num_multicast_groups) {
            rx.receive(boost::asio::buffer(&receiver_buffer, sizeof(receiver_buffer)));
            received.insert(receiver_buffer);
        }

        keep_going = false;
        tx_thead.join();
    }

    SECTION("Add a multicast stream") {
        auto receiver = std::make_unique<rav::rtp::Receiver3>();

        const auto multicast_addr = boost::asio::ip::make_address_v4("239.1.2.3");
        const auto src_addr = boost::asio::ip::make_address_v4("192.168.1.1");

        rav::rtp::Receiver3::ArrayOfSessions sessions {
            rav::rtp::Session {multicast_addr, 5004, 5005},
        };

        rav::rtp::Receiver3::ArrayOfFilters filters {
            rav::rtp::Filter {multicast_addr, src_addr, rav::sdp::FilterMode::include},
        };

        rav::rtp::Receiver3::ArrayOfAddresses interface_addresses {boost::asio::ip::make_address_v4("127.0.0.1")};
        auto result = receiver->add_stream(rav::Id(1), sessions, filters, interface_addresses, io_context);
        REQUIRE(result);

        REQUIRE(receiver->streams.size() == 1);
        REQUIRE(receiver->streams.at(0).id == rav::Id(1));
        REQUIRE(receiver->streams.at(0).sessions == sessions);
        REQUIRE(receiver->streams.at(0).filters == filters);

        REQUIRE(receiver->sockets.size() == 1);
        REQUIRE(receiver->sockets.at(0).interface_address == interface_addresses.at(0));
        REQUIRE(receiver->sockets.at(0).state == rav::rtp::Receiver3::State::ready);
        REQUIRE(receiver->sockets.at(0).connection_endpoint == boost::asio::ip::udp::endpoint(multicast_addr, 5004));
        REQUIRE(receiver->sockets.at(0).socket.is_open());
    }

    SECTION("Add a redundant multicast stream") {
        auto receiver = std::make_unique<rav::rtp::Receiver3>();

        const auto multicast_addr_pri = boost::asio::ip::make_address_v4("239.0.0.1");
        const auto multicast_addr_sec = boost::asio::ip::make_address_v4("239.0.0.2");

        const auto src_addr_pri = boost::asio::ip::make_address("192.168.1.1");
        const auto src_addr_sec = boost::asio::ip::make_address("192.168.1.2");

        rav::rtp::Receiver3::ArrayOfSessions sessions {
            rav::rtp::Session {multicast_addr_pri, 5004, 5005},
            rav::rtp::Session {multicast_addr_sec, 5004, 5005},
        };

        rav::rtp::Receiver3::ArrayOfFilters filters {
            rav::rtp::Filter {multicast_addr_pri, src_addr_pri, rav::sdp::FilterMode::include},
            rav::rtp::Filter {multicast_addr_sec, src_addr_sec, rav::sdp::FilterMode::include},
        };

        rav::rtp::Receiver3::ArrayOfAddresses interface_addresses {
            boost::asio::ip::make_address_v4("127.0.0.1"),
            boost::asio::ip::make_address_v4("127.0.0.1"),
        };
        auto result = receiver->add_stream(rav::Id(1), sessions, filters, interface_addresses, io_context);
        REQUIRE(result);

        REQUIRE(receiver->streams.size() == 1);
        REQUIRE(receiver->streams.at(0).id == rav::Id(1));
        REQUIRE(receiver->streams.at(0).sessions == sessions);
        REQUIRE(receiver->streams.at(0).filters == filters);

        REQUIRE(receiver->sockets.size() == 2);
        REQUIRE(receiver->sockets.at(0).interface_address == interface_addresses.at(0));
        REQUIRE(receiver->sockets.at(0).state == rav::rtp::Receiver3::State::ready);
        REQUIRE(
            receiver->sockets.at(0).connection_endpoint == boost::asio::ip::udp::endpoint(multicast_addr_pri, 5004)
        );
        REQUIRE(receiver->sockets.at(0).socket.is_open());
        REQUIRE(receiver->sockets.at(1).interface_address == interface_addresses.at(1));
        REQUIRE(receiver->sockets.at(1).state == rav::rtp::Receiver3::State::ready);
        REQUIRE(
            receiver->sockets.at(1).connection_endpoint == boost::asio::ip::udp::endpoint(multicast_addr_sec, 5004)
        );
        REQUIRE(receiver->sockets.at(1).socket.is_open());
    }
}
