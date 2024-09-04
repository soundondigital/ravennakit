/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <fmt/core.h>

#include <iostream>
#include <uvw.hpp>

#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/rtp/rtp_receiver.hpp"

constexpr short port = 5004;

int main(int const argc, char* argv[]) {
    if (argc < 2 || argc > 4) {
        std::cerr << "Usage: receiver <listen_address>\n";
        std::cerr << "  For IPv4, try:\n";
        std::cerr << "    unicast_receiver_example 0.0.0.0 [239.1.15.51] [192.168.15.52]\n";
        return 1;
    }

#ifdef RAV_ENABLE_SPDLOG
    spdlog::set_level(spdlog::level::trace);
#endif

    const auto loop = uvw::loop::create();
    if (loop == nullptr) {
        return 1;
    }

    rav::rtp_receiver receiver(loop);
    receiver.on<rav::rtp_packet_event>([](const rav::rtp_packet_event& event,
                                          [[maybe_unused]] rav::rtp_receiver& recv) {
        fmt::println("{}", event.packet.to_string());
    });
    receiver.on<rav::rtcp_packet_event>([](const rav::rtcp_packet_event& event,
                                           [[maybe_unused]] rav::rtp_receiver& recv) {
        fmt::println("{}", event.packet.to_string());
    });

    receiver.bind(argv[1], port);

    if (argc == 4) {
        receiver.set_multicast_membership(argv[2], argv[3], uvw::udp_handle::membership::JOIN_GROUP);
    } else if (argc == 3) {
        receiver.set_multicast_membership(argv[2], "", uvw::udp_handle::membership::JOIN_GROUP);
    }

    receiver.start();

    const auto signal = loop->resource<uvw::signal_handle>();
    signal->on<uvw::signal_event>([&receiver, &signal](const uvw::signal_event&, uvw::signal_handle&) {
        receiver.close();
        signal->close();  // Need to close ourselves, otherwise the loop will not stop.
    });
    signal->start(SIGTERM);

    return loop->run();
}
