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
#include "ravennakit/core/net/interfaces/network_interface_list.hpp"

#include <catch2/catch_all.hpp>

#include <thread>

namespace {
using MulticastMembershipChangesVector =
    std::vector<std::tuple<bool, uint16_t, boost::asio::ip::address_v4, boost::asio::ip::address_v4>>;

void setup_receiver_multicast_hooks(rav::rtp::Receiver3& receiver, MulticastMembershipChangesVector& changes) {
    receiver.join_multicast_group = [&changes](
                                        const boost::asio::ip::udp::socket& socket,
                                        const boost::asio::ip::address_v4& multicast_group,
                                        const boost::asio::ip::address_v4& interface_address
                                    ) {
        changes.emplace_back(true, socket.local_endpoint().port(), multicast_group, interface_address);
        return true;
    };
    receiver.leave_multicast_group = [&changes](
                                         const boost::asio::ip::udp::socket& socket,
                                         const boost::asio::ip::address_v4& multicast_group,
                                         const boost::asio::ip::address_v4& interface_address
                                     ) {
        changes.emplace_back(false, socket.local_endpoint().port(), multicast_group, interface_address);
        return true;
    };
}

}  // namespace

TEST_CASE("rav::rtp::Receiver") {
    boost::asio::io_context io_context;

    SECTION("Test bounds") {
        REQUIRE(rav::rtp::Receiver3::k_max_num_readers >= 1);
        REQUIRE(rav::rtp::Receiver3::k_max_num_redundant_sessions >= 1);
        REQUIRE(
            rav::rtp::Receiver3::k_max_num_sessions
            == rav::rtp::Receiver3::k_max_num_readers * rav::rtp::Receiver3::k_max_num_redundant_sessions
        );
    }

    SECTION("Initial state") {
        auto receiver = std::make_unique<rav::rtp::Receiver3>();

        // Sockets
        REQUIRE(decltype(receiver->sockets)::capacity() == rav::rtp::Receiver3::k_max_num_sessions);
        REQUIRE(receiver->sockets.empty());

        // Streams
        REQUIRE(decltype(receiver->readers)::capacity() == rav::rtp::Receiver3::k_max_num_readers);
        REQUIRE(receiver->readers.empty());
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
        auto interface_address = boost::asio::ip::address_v4::loopback();
#if RAV_WINDOWS
        auto iface = rav::NetworkInterfaceList::get_system_interfaces().find_by_type(
            rav::NetworkInterface::Type::wired_ethernet
        );
        if (iface != nullptr) {
            interface_address = iface->get_first_ipv4_address();
        }
#endif
        auto multicast_base_address = boost::asio::ip::make_address_v4("239.0.0.1");
        static constexpr uint32_t k_num_multicast_groups = 1;

#if RAV_WINDOWS
        boost::asio::ip::udp::socket rx(io_context, {interface_address, 0});
#else
        boost::asio::ip::udp::socket rx(io_context, {boost::asio::ip::address_v4::any(), 0});
#endif

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

        std::thread rx_thead([&rx] {
            std::set<uint32_t> received;
            uint32_t receiver_buffer = 0;

            while (received.size() < k_num_multicast_groups) {
                rx.receive(boost::asio::buffer(&receiver_buffer, sizeof(receiver_buffer)));
                received.insert(receiver_buffer);
            }
        });

        // Give rx_thread time to get going
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Join the groups here to test thead safely (by enabling TSAN)
        for (uint32_t i = 0; i < k_num_multicast_groups; ++i) {
            boost::asio::ip::address_v4 addr(multicast_base_address.to_uint() + i);
            rx.set_option(boost::asio::ip::multicast::join_group(addr, interface_address));
        }

        rx_thead.join();
        keep_going = false;
        tx_thead.join();
    }

    SECTION("Add a multicast stream") {
        const auto multicast_addr = boost::asio::ip::make_address_v4("239.1.2.3");
        const auto src_addr = boost::asio::ip::make_address_v4("192.168.1.1");
        const auto interface_address = boost::asio::ip::address_v4::loopback();

        rav::rtp::Receiver3::ArrayOfSessions sessions {
            rav::rtp::Session {multicast_addr, 5004, 5005},
        };

        rav::rtp::Receiver3::ArrayOfFilters filters {
            rav::rtp::Filter {multicast_addr, src_addr, rav::sdp::FilterMode::include},
        };

        rav::rtp::Receiver3::ArrayOfAddresses interface_addresses {interface_address};

        auto receiver = std::make_unique<rav::rtp::Receiver3>();
        MulticastMembershipChangesVector multicast_group_membership_changes;
        setup_receiver_multicast_hooks(*receiver, multicast_group_membership_changes);
        receiver->set_interface_addresses(interface_addresses);

        auto result = receiver->add_reader(rav::Id(1), sessions, filters, io_context);
        REQUIRE(result);

        REQUIRE(receiver->readers.size() == 1);
        REQUIRE(receiver->readers.at(0).id == rav::Id(1));
        REQUIRE(receiver->readers.at(0).sessions == sessions);
        REQUIRE(receiver->readers.at(0).filters == filters);

        REQUIRE(receiver->sockets.size() == 1);
        REQUIRE(receiver->sockets.at(0).port == 5004);
        REQUIRE(receiver->sockets.at(0).socket.is_open());

        REQUIRE(multicast_group_membership_changes.size() == 1);
        REQUIRE(
            multicast_group_membership_changes[0]
            == std::tuple(true, 5004, boost::asio::ip::make_address_v4("239.1.2.3"), interface_address)
        );

        result = receiver->remove_reader(rav::Id(1));
        REQUIRE(result);
    }

    SECTION("Add and remove streams") {
        auto receiver = std::make_unique<rav::rtp::Receiver3>();

        const auto multicast_addr_pri = boost::asio::ip::make_address_v4("239.0.0.1");
        const auto multicast_addr_sec = boost::asio::ip::make_address_v4("239.0.0.2");

        const auto src_addr_pri = boost::asio::ip::make_address("192.168.1.1");
        const auto src_addr_sec = boost::asio::ip::make_address("192.168.1.2");

        const auto interface_address_pri = boost::asio::ip::make_address_v4("192.168.1.3");
        const auto interface_address_sec = boost::asio::ip::make_address_v4("192.168.1.4");

        rav::rtp::Receiver3::ArrayOfSessions sessions {
            rav::rtp::Session {multicast_addr_pri, 5004, 5005},
            rav::rtp::Session {multicast_addr_sec, 5004, 5005},
        };

        rav::rtp::Receiver3::ArrayOfFilters filters {
            rav::rtp::Filter {multicast_addr_pri, src_addr_pri, rav::sdp::FilterMode::include},
            rav::rtp::Filter {multicast_addr_sec, src_addr_sec, rav::sdp::FilterMode::include},
        };

        rav::rtp::Receiver3::ArrayOfAddresses interface_addresses {
            interface_address_pri,
            interface_address_sec,
        };

        MulticastMembershipChangesVector membership_changes;
        setup_receiver_multicast_hooks(*receiver, membership_changes);

        receiver->set_interface_addresses(interface_addresses);
        auto result = receiver->add_reader(rav::Id(1), sessions, filters, io_context);
        REQUIRE(result);

        REQUIRE(receiver->readers.size() == 1);
        REQUIRE(receiver->readers.at(0).id == rav::Id(1));
        REQUIRE(receiver->readers.at(0).sessions == sessions);
        REQUIRE(receiver->readers.at(0).filters == filters);

        REQUIRE(receiver->sockets.size() == 1);
        REQUIRE(receiver->sockets.at(0).port == 5004);
        REQUIRE(receiver->sockets.at(0).socket.is_open());

        REQUIRE(membership_changes.size() == 2);
        REQUIRE(membership_changes[0] == std::tuple(true, 5004, multicast_addr_pri, interface_address_pri));
        REQUIRE(membership_changes[1] == std::tuple(true, 5004, multicast_addr_sec, interface_address_sec));

        // Add a second reader with the same sessions
        result = receiver->add_reader(rav::Id(2), sessions, filters, io_context);
        REQUIRE(result);
        REQUIRE(receiver->readers.size() == 2);
        REQUIRE(receiver->sockets.size() == 1);
        REQUIRE(membership_changes.size() == 2);
        REQUIRE(receiver->readers.at(1).id == rav::Id(2));

        // Add a 3rd reader with the different ports
        sessions[0].rtp_port = 5006;
        sessions[0].rtcp_port = 5007;
        sessions[1].rtp_port = 5008;
        sessions[1].rtcp_port = 5009;
        result = receiver->add_reader(rav::Id(3), sessions, filters, io_context);
        REQUIRE(result);

        REQUIRE(receiver->readers.size() == 3);
        REQUIRE(receiver->readers.at(2).id == rav::Id(3));
        REQUIRE(receiver->readers.at(2).sessions == sessions);
        REQUIRE(receiver->readers.at(2).filters == filters);

        REQUIRE(receiver->sockets.size() == 3);
        REQUIRE(receiver->sockets.at(1).port == 5006);
        REQUIRE(receiver->sockets.at(1).socket.is_open());
        REQUIRE(receiver->sockets.at(2).port == 5008);
        REQUIRE(receiver->sockets.at(2).socket.is_open());

        REQUIRE(membership_changes.size() == 4);
        REQUIRE(membership_changes[2] == std::tuple(true, 5006, multicast_addr_pri, interface_address_pri));
        REQUIRE(membership_changes[3] == std::tuple(true, 5008, multicast_addr_sec, interface_address_sec));

        // Remove reader 2
        result = receiver->remove_reader(rav::Id(2));
        REQUIRE(result);
        REQUIRE(receiver->sockets.size() == 3);  // Size of sockets never shrinks
        REQUIRE(receiver->sockets[0].socket.is_open());
        REQUIRE(receiver->readers.size() == 3);            // Size of readers never shrinks
        REQUIRE(receiver->readers.at(1).id == rav::Id());  // The reader slot should have been invalidated
        REQUIRE(membership_changes.size() == 4);

        // Remove reader 1
        result = receiver->remove_reader(rav::Id(1));
        REQUIRE(result);
        REQUIRE(receiver->sockets.size() == 3);  // Size of sockets never shrinks
        REQUIRE_FALSE(receiver->sockets[0].socket.is_open());
        REQUIRE(receiver->readers.size() == 3);            // Size of readers never shrinks
        REQUIRE(receiver->readers.at(0).id == rav::Id());  // The reader slot should have been invalidated
        REQUIRE(membership_changes.size() == 6);
        REQUIRE(membership_changes[4] == std::tuple(false, 5004, multicast_addr_pri, interface_address_pri));
        REQUIRE(membership_changes[5] == std::tuple(false, 5004, multicast_addr_sec, interface_address_sec));

        // Remove reader 3
        result = receiver->remove_reader(rav::Id(3));
        REQUIRE(result);
        REQUIRE(receiver->sockets.size() == 3);  // Size of sockets never shrinks
        REQUIRE_FALSE(receiver->sockets[1].socket.is_open());
        REQUIRE_FALSE(receiver->sockets[2].socket.is_open());
        REQUIRE(receiver->readers.size() == 3);  // Size of readers never shrinks
        REQUIRE(receiver->readers.at(2).id == rav::Id());
        REQUIRE(membership_changes.size() == 8);
        REQUIRE(membership_changes[6] == std::tuple(false, 5006, multicast_addr_pri, interface_address_pri));
        REQUIRE(membership_changes[7] == std::tuple(false, 5008, multicast_addr_sec, interface_address_sec));
    }
}
