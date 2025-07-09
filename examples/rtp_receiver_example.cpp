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
    const auto multicast_addr_pri = boost::asio::ip::make_address("239.1.11.54");
    const auto multicast_addr_sec = boost::asio::ip::make_address("239.1.12.54");

    const rav::rtp::Receiver3::ArrayOfAddresses interface_addresses {
        boost::asio::ip::make_address_v4("192.168.11.51"),
        boost::asio::ip::make_address_v4("192.168.12.51"),
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
    rav::rtp::Receiver3 receiver;
    receiver.add_stream(rav::Id(1), sessions, filters, interface_addresses, io_context);

    while (true) {
        receiver.do_high_prio_processing();

        rav::rtp::Receiver3::PacketBuffer buffer;
        for (auto& stream : receiver.streams) {
            if (stream.fifo.pop(buffer)) {
                rav::rtp::PacketView view(buffer.data(), buffer.size());
                fmt::println("{}", view.to_string());
            }
        }
    }

    return 0;
}
