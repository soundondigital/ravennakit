/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/detail/rtp_audio_receiver.hpp"
#include "ravennakit/core/net/interfaces/network_interface_list.hpp"
#include "ravennakit/core/util/defer.hpp"

#include <catch2/catch_all.hpp>

#include <thread>

namespace {
using MulticastMembershipChangesVector =
    std::vector<std::tuple<bool, uint16_t, boost::asio::ip::address_v4, boost::asio::ip::address_v4>>;

[[maybe_unused]] std::string to_string(const MulticastMembershipChangesVector& changes) {
    std::string result;
    for (auto& change : changes) {
        fmt::format_to(
            std::back_inserter(result), "{} {}:{} on {}\n", std::get<0>(change) ? "joined" : "left",
            std::get<1>(change), std::get<2>(change).to_string(), std::get<3>(change).to_string()
        );
    }
    return result;
}

void setup_receiver_multicast_hooks(rav::rtp::AudioReceiver& receiver, MulticastMembershipChangesVector& changes) {
    receiver.join_multicast_group = [&changes](
                                        const boost::asio::ip::udp::socket& socket,
                                        const boost::asio::ip::address_v4& multicast_group,
                                        const boost::asio::ip::address_v4& interface_address
                                    ) {
        REQUIRE(socket.is_open());
        changes.emplace_back(true, socket.local_endpoint().port(), multicast_group, interface_address);
        return true;
    };
    receiver.leave_multicast_group = [&changes](
                                         const boost::asio::ip::udp::socket& socket,
                                         const boost::asio::ip::address_v4& multicast_group,
                                         const boost::asio::ip::address_v4& interface_address
                                     ) {
        REQUIRE(socket.is_open());
        changes.emplace_back(false, socket.local_endpoint().port(), multicast_group, interface_address);
        return true;
    };
}

[[nodiscard]] size_t count_valid_readers(rav::rtp::AudioReceiver& receiver) {
    size_t count = 0;
    for (auto& reader : receiver.readers) {
        if (reader.id.is_valid()) {
            count++;
        }
    }
    return count;
}

[[nodiscard]] size_t count_open_sockets(rav::rtp::AudioReceiver& receiver) {
    size_t count = 0;
    for (auto& socket : receiver.sockets) {
        if (socket.socket.is_open()) {
            count++;
        }
    }
    return count;
}

}  // namespace

TEST_CASE("rav::rtp::Receiver") {
    boost::asio::io_context io_context;

    SECTION("Test bounds") {
        REQUIRE(rav::rtp::AudioReceiver::k_max_num_readers >= 1);
        REQUIRE(rav::rtp::AudioReceiver::k_max_num_redundant_sessions >= 1);
        REQUIRE(
            rav::rtp::AudioReceiver::k_max_num_sessions
            == rav::rtp::AudioReceiver::k_max_num_readers * rav::rtp::AudioReceiver::k_max_num_redundant_sessions
        );
    }

    SECTION("Initial state") {
        auto receiver = std::make_unique<rav::rtp::AudioReceiver>(io_context);

        // Sockets
        REQUIRE(decltype(receiver->sockets)::capacity() == rav::rtp::AudioReceiver::k_max_num_sessions);
        REQUIRE(receiver->sockets.size() == rav::rtp::AudioReceiver::k_max_num_sessions);

        // Streams
        REQUIRE(decltype(receiver->readers)::capacity() == rav::rtp::AudioReceiver::k_max_num_readers);
        REQUIRE(receiver->readers.size() == rav::rtp::AudioReceiver::k_max_num_readers);
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

    rav::AudioFormat audio_format {
        rav::AudioFormat::ByteOrder::be,
        rav::AudioEncoding::pcm_s24,
        rav::AudioFormat::ChannelOrdering::interleaved,
        48000,
        2,
    };

    SECTION("Add a multicast stream") {
        const auto multicast_addr = boost::asio::ip::make_address_v4("239.1.2.3");
        const auto src_addr = boost::asio::ip::make_address_v4("192.168.1.1");
        const auto interface_address = boost::asio::ip::address_v4::loopback();

        rav::rtp::AudioReceiver::ArrayOfAddresses interface_addresses {interface_address};

        auto receiver = std::make_unique<rav::rtp::AudioReceiver>(io_context);
        MulticastMembershipChangesVector multicast_group_membership_changes;
        setup_receiver_multicast_hooks(*receiver, multicast_group_membership_changes);
        REQUIRE(receiver->set_interfaces(interface_addresses));

        rav::rtp::AudioReceiver::StreamInfo stream {
            rav::rtp::Session {multicast_addr, 5004, 5005},
            rav::rtp::Filter {multicast_addr, src_addr, rav::sdp::FilterMode::include},
        };

        rav::rtp::AudioReceiver::ReaderParameters parameters {audio_format, {stream}};
        parameters.streams[0] = stream;

        auto result = receiver->add_reader(rav::Id(1), parameters, interface_addresses);
        REQUIRE(result);

        REQUIRE(count_valid_readers(*receiver) == 1);
        const auto& reader = receiver->readers.at(0);
        REQUIRE(reader.id == rav::Id(1));
        REQUIRE(reader.streams.at(0).session == parameters.streams.at(0).session);
        REQUIRE(reader.streams.at(0).filter == parameters.streams.at(0).filter);
        REQUIRE(reader.streams.at(0).packet_time_frames == parameters.streams.at(0).packet_time_frames);

        REQUIRE(count_open_sockets(*receiver) == 1);
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
        auto receiver = std::make_unique<rav::rtp::AudioReceiver>(io_context);

        const auto multicast_addr_pri = boost::asio::ip::make_address_v4("239.0.0.1");
        const auto multicast_addr_sec = boost::asio::ip::make_address_v4("239.0.0.2");

        const auto src_addr_pri = boost::asio::ip::make_address("192.168.1.1");
        const auto src_addr_sec = boost::asio::ip::make_address("192.168.1.2");

        const auto interface_address_pri = boost::asio::ip::make_address_v4("192.168.1.3");
        const auto interface_address_sec = boost::asio::ip::make_address_v4("192.168.1.4");

        rav::rtp::AudioReceiver::StreamInfo stream_pri {
            rav::rtp::Session {multicast_addr_pri, 5004, 5005},
            rav::rtp::Filter {multicast_addr_pri, src_addr_pri, rav::sdp::FilterMode::include},
            48,
        };

        rav::rtp::AudioReceiver::StreamInfo stream_sec {
            rav::rtp::Session {multicast_addr_sec, 5004, 5005},
            rav::rtp::Filter {multicast_addr_sec, src_addr_sec, rav::sdp::FilterMode::include},
            48,
        };

        rav::rtp::AudioReceiver::ReaderParameters parameters {audio_format, {stream_pri, stream_sec}};

        rav::rtp::AudioReceiver::ArrayOfAddresses interface_addresses {
            interface_address_pri,
            interface_address_sec,
        };

        MulticastMembershipChangesVector membership_changes;
        setup_receiver_multicast_hooks(*receiver, membership_changes);

        REQUIRE(receiver->set_interfaces(interface_addresses));
        auto result = receiver->add_reader(rav::Id(1), parameters, interface_addresses);
        REQUIRE(result);

        REQUIRE(count_valid_readers(*receiver) == 1);
        REQUIRE(receiver->readers.at(0).id == rav::Id(1));
        REQUIRE(receiver->readers.at(0).streams[0].session == stream_pri.session);
        REQUIRE(receiver->readers.at(0).streams[0].filter == stream_pri.filter);
        REQUIRE(receiver->readers.at(0).streams[0].packet_time_frames == stream_pri.packet_time_frames);
        REQUIRE(receiver->readers.at(0).streams[1].session == stream_sec.session);
        REQUIRE(receiver->readers.at(0).streams[1].filter == stream_sec.filter);
        REQUIRE(receiver->readers.at(0).streams[1].packet_time_frames == stream_sec.packet_time_frames);

        REQUIRE(count_open_sockets(*receiver) == 1);
        REQUIRE(receiver->sockets.at(0).port == 5004);
        REQUIRE(receiver->sockets.at(0).socket.is_open());

        REQUIRE(membership_changes.size() == 2);
        REQUIRE(membership_changes[0] == std::tuple(true, 5004, multicast_addr_pri, interface_address_pri));
        REQUIRE(membership_changes[1] == std::tuple(true, 5004, multicast_addr_sec, interface_address_sec));

        // Add a second reader with the same sessions
        result = receiver->add_reader(rav::Id(2), parameters, interface_addresses);
        REQUIRE(result);
        REQUIRE(count_valid_readers(*receiver) == 2);
        REQUIRE(count_open_sockets(*receiver) == 1);
        REQUIRE(membership_changes.size() == 2);
        REQUIRE(receiver->readers.at(1).id == rav::Id(2));

        // Add a 3rd reader with the different ports
        parameters.streams[0].session.rtp_port = 5006;
        parameters.streams[0].session.rtcp_port = 5007;
        parameters.streams[1].session.rtp_port = 5008;
        parameters.streams[1].session.rtcp_port = 5009;
        result = receiver->add_reader(rav::Id(3), parameters, interface_addresses);
        REQUIRE(result);

        REQUIRE(count_valid_readers(*receiver) == 3);
        REQUIRE(receiver->readers.at(2).id == rav::Id(3));
        REQUIRE(receiver->readers.at(2).streams[0].session == parameters.streams[0].session);
        REQUIRE(receiver->readers.at(2).streams[0].filter == parameters.streams[0].filter);
        REQUIRE(receiver->readers.at(2).streams[0].packet_time_frames == parameters.streams[0].packet_time_frames);
        REQUIRE(receiver->readers.at(2).streams[1].session == parameters.streams[1].session);
        REQUIRE(receiver->readers.at(2).streams[1].filter == parameters.streams[1].filter);
        REQUIRE(receiver->readers.at(2).streams[1].packet_time_frames == parameters.streams[1].packet_time_frames);

        REQUIRE(count_open_sockets(*receiver) == 3);
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
        REQUIRE(count_open_sockets(*receiver) == 3);  // Size of sockets never shrinks
        REQUIRE(receiver->sockets[0].socket.is_open());
        REQUIRE(receiver->readers.size() == receiver->readers.capacity());  // Size of readers never shrinks
        REQUIRE(receiver->readers.at(1).id == rav::Id());  // The reader slot should have been invalidated
        REQUIRE(membership_changes.size() == 4);

        // Remove reader 1
        result = receiver->remove_reader(rav::Id(1));
        REQUIRE(result);
        REQUIRE(receiver->sockets.size() == receiver->sockets.capacity());  // Size of sockets never shrinks
        REQUIRE_FALSE(receiver->sockets[0].socket.is_open());
        REQUIRE(receiver->readers.size() == receiver->readers.capacity());  // Size of readers never shrinks
        REQUIRE(receiver->readers.at(0).id == rav::Id());  // The reader slot should have been invalidated
        REQUIRE(membership_changes.size() == 6);
        REQUIRE(membership_changes[4] == std::tuple(false, 5004, multicast_addr_pri, interface_address_pri));
        REQUIRE(membership_changes[5] == std::tuple(false, 5004, multicast_addr_sec, interface_address_sec));

        // Remove reader 3
        result = receiver->remove_reader(rav::Id(3));
        REQUIRE(result);
        REQUIRE(receiver->sockets.size() == receiver->sockets.capacity());  // Size of sockets never shrinks
        REQUIRE_FALSE(receiver->sockets[1].socket.is_open());
        REQUIRE_FALSE(receiver->sockets[2].socket.is_open());
        REQUIRE(receiver->readers.size() == receiver->readers.capacity());  // Size of readers never shrinks
        REQUIRE(receiver->readers.at(2).id == rav::Id());
        REQUIRE(membership_changes.size() == 8);
        REQUIRE(membership_changes[6] == std::tuple(false, 5006, multicast_addr_pri, interface_address_pri));
        REQUIRE(membership_changes[7] == std::tuple(false, 5008, multicast_addr_sec, interface_address_sec));
    }

    SECTION("Set interfaces") {
        auto receiver = std::make_unique<rav::rtp::AudioReceiver>(io_context);

        const auto multicast_addr_pri = boost::asio::ip::make_address_v4("239.0.0.1");
        const auto multicast_addr_sec = boost::asio::ip::make_address_v4("239.0.0.2");

        const auto interface_address_pri = boost::asio::ip::make_address_v4("192.168.1.1");
        const auto interface_address_sec = boost::asio::ip::make_address_v4("192.168.1.2");

        rav::rtp::AudioReceiver::StreamInfo stream_pri {
            rav::rtp::Session {multicast_addr_pri, 5004, 5005},
            rav::rtp::Filter {multicast_addr_pri},
            48,
        };

        rav::rtp::AudioReceiver::StreamInfo stream_sec {
            rav::rtp::Session {multicast_addr_sec, 5004, 5005},
            rav::rtp::Filter {multicast_addr_sec},
            48,
        };

        rav::rtp::AudioReceiver::ReaderParameters parameters {audio_format, {stream_pri, stream_sec}};

        rav::rtp::AudioReceiver::ArrayOfAddresses interface_addresses {
            interface_address_pri,
            interface_address_sec,
        };

        MulticastMembershipChangesVector membership_changes;
        setup_receiver_multicast_hooks(*receiver, membership_changes);

        auto id = rav::Id(1);
        auto result = receiver->add_reader(id, parameters, {});
        REQUIRE(result);

        rav::Defer remove_reader([&] {
            REQUIRE(receiver->remove_reader(id));
        });

        REQUIRE(membership_changes.empty());

        REQUIRE(receiver->set_interfaces(interface_addresses));

        REQUIRE(membership_changes.size() == 2);
        REQUIRE(membership_changes[0] == std::tuple(true, 5004, multicast_addr_pri, interface_address_pri));
        REQUIRE(membership_changes[1] == std::tuple(true, 5004, multicast_addr_sec, interface_address_sec));

        SECTION("Swap interfaces") {
            std::swap(interface_addresses[0], interface_addresses[1]);
            REQUIRE(receiver->set_interfaces(interface_addresses));
            REQUIRE(membership_changes.size() == 6);
            REQUIRE(membership_changes[2] == std::tuple(false, 5004, multicast_addr_pri, interface_address_pri));
            REQUIRE(membership_changes[3] == std::tuple(true, 5004, multicast_addr_pri, interface_address_sec));
            REQUIRE(membership_changes[4] == std::tuple(false, 5004, multicast_addr_sec, interface_address_sec));
            REQUIRE(membership_changes[5] == std::tuple(true, 5004, multicast_addr_sec, interface_address_pri));
        }

        SECTION("Clear interfaces") {
            REQUIRE(receiver->set_interfaces({}));

            REQUIRE(membership_changes.size() == 4);
            REQUIRE(membership_changes[2] == std::tuple(false, 5004, multicast_addr_pri, interface_address_pri));
            REQUIRE(membership_changes[3] == std::tuple(false, 5004, multicast_addr_sec, interface_address_sec));

            // Clearing interfaces should not lead to closing sockets
            REQUIRE(count_open_sockets(*receiver) == 1);
            REQUIRE(receiver->sockets[0].socket.is_open());
        }
    }

    SECTION("Adding, removing and adding reader for the same port should re-open an existing slot") {
        const auto multicast_addr = boost::asio::ip::make_address_v4("239.1.2.3");
        const auto src_addr = boost::asio::ip::make_address_v4("192.168.1.1");
        const auto interface_address = boost::asio::ip::address_v4::loopback();

        rav::rtp::AudioReceiver::ArrayOfAddresses interface_addresses {interface_address};

        auto receiver = std::make_unique<rav::rtp::AudioReceiver>(io_context);

        rav::rtp::AudioReceiver::StreamInfo stream {
            rav::rtp::Session {multicast_addr, 5004, 5005},
            rav::rtp::Filter {multicast_addr, src_addr, rav::sdp::FilterMode::include},
        };

        rav::rtp::AudioReceiver::ReaderParameters parameters {audio_format, {stream}};
        parameters.streams[0] = stream;

        auto result = receiver->add_reader(rav::Id(1), parameters, interface_addresses);
        REQUIRE(result);

        REQUIRE(count_valid_readers(*receiver) == 1);
        REQUIRE(count_open_sockets(*receiver) == 1);

        REQUIRE(receiver->remove_reader(rav::Id(1)));

        REQUIRE(count_valid_readers(*receiver) == 0);
        REQUIRE(count_open_sockets(*receiver) == 0);

        // All sockets should have been reset
        for (auto& socket : receiver->sockets) {
            REQUIRE(socket.port == 0);
        }

        result = receiver->add_reader(rav::Id(1), parameters, interface_addresses);
        REQUIRE(result);

        REQUIRE(count_valid_readers(*receiver) == 1);
        REQUIRE(count_open_sockets(*receiver) == 1);

        REQUIRE(receiver->remove_reader(rav::Id(1)));
    }
}
