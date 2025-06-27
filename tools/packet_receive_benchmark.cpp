/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/clock.hpp"
#include "ravennakit/core/math/sliding_stats.hpp"
#include "ravennakit/core/platform/apple/priority.hpp"
#include "ravennakit/core/util/wrapping_uint.hpp"
#include "ravennakit/core/util/tracy.hpp"

#include <boost/asio.hpp>
#include <fmt/format.h>
#include <array>

using boost::asio::ip::udp;

namespace {
constexpr auto k_num_packets = 20000;

struct State {
    boost::asio::io_context io_context;
    udp::socket socket {io_context};
    std::array<char, 1500> buffer {};
    udp::endpoint sender_endpoint;
    rav::SlidingStats stats {k_num_packets};
    rav::WrappingUint64 previous_packet_time;
};

void restart(State& state) {
    state.stats.reset();
    state.previous_packet_time = {};
    state.io_context.restart();
}

void handle_received_packet(State& state) {
    TRACY_ZONE_SCOPED;

    if (state.previous_packet_time.value() == 0) {
        state.previous_packet_time.update(rav::clock::now_monotonic_high_resolution_ns());
    } else {
        if (const auto diff = state.previous_packet_time.update(rav::clock::now_monotonic_high_resolution_ns())) {
            const auto interval = static_cast<double>(*diff) / 1'000'000;
            TRACY_PLOT("Packet interval", interval);
            state.stats.add(interval);
        }
    }
}

void async_receive_from(State& state) {
    state.socket.async_receive_from(
        boost::asio::buffer(state.buffer), state.sender_endpoint,
        [&state](const std::error_code ec, std::size_t) {
            TRACY_ZONE_SCOPED;

            if (ec) {
                throw std::system_error(ec);
            }

            if (state.stats.full()) {
                return;
            }

            handle_received_packet(state);
            async_receive_from(state);
        }
    );
}

}  // namespace

int main() {
    const auto multicast_address = boost::asio::ip::make_address_v4("239.1.11.54");
    const auto interface_address = boost::asio::ip::make_address_v4("192.168.11.51");
    constexpr unsigned short multicast_port = 5004;
    const udp::endpoint listen_endpoint(udp::v4(), multicast_port);

    State state;
    state.socket.open(listen_endpoint.protocol());
    state.socket.set_option(udp::socket::reuse_address(true));
    state.socket.bind(listen_endpoint);
    state.socket.set_option(boost::asio::ip::multicast::join_group(multicast_address, interface_address));
    state.socket.non_blocking(true);

    // Run
    TRACY_MESSAGE("io_context::run");
    restart(state);
    async_receive_from(state);
    state.io_context.run();
    fmt::println("Stats for io_context.run() : {}", state.stats.to_string());

    // Poll
    TRACY_MESSAGE("io_context::poll");
    restart(state);
    async_receive_from(state);
    while (!state.io_context.stopped()) {
        state.io_context.poll();
    }
    fmt::println("Stats for io_context.poll(): {}", state.stats.to_string());

    // Hammer
    TRACY_MESSAGE("hammer");
    restart(state);
    while (!state.stats.full()) {
        udp::endpoint sender_endpoint;
        boost::system::error_code ec;
        const auto size = state.socket.receive_from(boost::asio::buffer(state.buffer), sender_endpoint, 0, ec);
        if (!ec && size > 0) {
            handle_received_packet(state);
        }
        std::this_thread::yield();
    }
    fmt::println("Stats for hammering: {}", state.stats.to_string());

#if RAV_APPLE
    constexpr auto min_packet_time = 125 * 1000;       // 125us
    constexpr auto max_packet_time = 4 * 1000 * 1000;  // 4ms
    if (!rav::set_thread_realtime(min_packet_time, max_packet_time, max_packet_time * 2)) {
        throw std::runtime_error("Failed to set thread realtime");
    }
#endif

    // Run
    TRACY_MESSAGE("io_context::run high prio");
    restart(state);
    async_receive_from(state);
    state.io_context.run();
    fmt::println("Stats for io_context.run() (high prio) : {}", state.stats.to_string());

    // Poll
    TRACY_MESSAGE("io_context::poll high prio");
    restart(state);
    async_receive_from(state);
    while (!state.io_context.stopped()) {
        state.io_context.poll();
    }
    fmt::println("Stats for io_context.poll() (high prio): {}", state.stats.to_string());

    // Hammer
    TRACY_MESSAGE("hammer high prio");
    restart(state);
    while (!state.stats.full()) {
        udp::endpoint sender_endpoint;
        boost::system::error_code ec;
        const auto size = state.socket.receive_from(boost::asio::buffer(state.buffer), sender_endpoint, 0, ec);
        if (!ec && size > 0) {
            handle_received_packet(state);
        }
        std::this_thread::yield();
    }
    fmt::println("Stats for hammering (high prio): {}", state.stats.to_string());

    return 0;
}
