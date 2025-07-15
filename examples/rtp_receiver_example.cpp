/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/util/id.hpp"
#include "ravennakit/nmos/nmos_node.hpp"
#include "ravennakit/rtp/rtp_audio_receiver.hpp"

#include <boost/asio.hpp>

[[noreturn]] int main() {
    const auto multicast_addr_pri = boost::asio::ip::make_address("239.15.1.5");
    const auto multicast_addr_sec = boost::asio::ip::make_address("239.16.1.6");

    const rav::rtp::Receiver3::ArrayOfAddresses interfaces {
        boost::asio::ip::make_address_v4("192.168.15.53"),
        boost::asio::ip::make_address_v4("192.168.16.50"),
    };

    const rav::rtp::Receiver3::ArrayOfSessions sessions {
        rav::rtp::Session {multicast_addr_pri, 5004, 5005},
        rav::rtp::Session {multicast_addr_sec, 5004, 5005},
    };

    const rav::rtp::Receiver3::ArrayOfFilters filters {
        rav::rtp::Filter {multicast_addr_pri},
        rav::rtp::Filter {multicast_addr_sec},
    };

    boost::asio::io_context io_context;
    const auto receiver = std::make_unique<rav::rtp::Receiver3>();
    std::ignore = receiver->add_reader(rav::Id(1), sessions, filters, interfaces, io_context);

    boost::asio::ip::udp::socket rx(io_context, {multicast_addr_pri, 0});

    while (true) {
        receiver->read_incoming_packets();

        rav::rtp::Receiver3::PacketBuffer buffer;
        for (auto& stream : receiver->readers) {
            while (stream.fifo.pop(buffer)) {
                rav::rtp::PacketView view(buffer.data(), buffer.size());
            }
        }
    }
}
