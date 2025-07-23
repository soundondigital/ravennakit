/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/platform.hpp"

#include <iostream>

#if RAV_POSIX

#include "ravennakit/core/clock.hpp"
#include "ravennakit/core/platform/apple/priority.hpp"
#include "ravennakit/core/util/tracy.hpp"
#include "ravennakit/core/util/wrapping_uint.hpp"

#include <iostream>
#include <cstring>
#include <ifaddrs.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fmt/format.h>
#include <netinet/in.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <thread>

constexpr int PORT = 5004;
constexpr size_t BUFFER_SIZE = 1500;
constexpr size_t NUM_PACKETS = 20000;
constexpr auto MULTICAST_GROUP = "239.1.11.54";
constexpr auto INTERFACE_IP = "192.168.11.51";

namespace {

struct State {
    rav::WrappingUint64 previous_packet_time;
    uint64_t count {};
    uint64_t max {};
};

void handle_received_packet(State& state) {
    TRACY_ZONE_SCOPED;

    if (state.previous_packet_time.value() == 0) {
        state.previous_packet_time.update(rav::clock::now_monotonic_high_resolution_ns());
    } else {
        if (const auto diff = state.previous_packet_time.update(rav::clock::now_monotonic_high_resolution_ns())) {
            TRACY_PLOT("Packet interval", static_cast<double>(*diff) / 1'000'000);
            state.max = std::max(state.max, *diff);
        }
    }

    state.count++;
}

std::string to_string(State& state) {
    return fmt::format("max={}ms, count={}", static_cast<double>(state.max) / 1'000'000, state.count);
}

}  // namespace

int main() {
    in_addr iface_addr {};
    if (inet_pton(AF_INET, INTERFACE_IP, &iface_addr) != 1) {
        std::cerr << "Invalid interface IP address\n";
        return 1;
    }

    const int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    constexpr int reuse = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt SO_REUSEADDR");
        close(fd);
        return 1;
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK) != 0) {
        perror("fcntl");
        close(fd);
        return 1;
    }

    sockaddr_in local_addr {};
    local_addr.sin_family = AF_INET;
    local_addr.sin_port = htons(PORT);
    local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, reinterpret_cast<sockaddr*>(&local_addr), sizeof(local_addr)) < 0) {
        perror("bind");
        close(fd);
        return 1;
    }

    ip_mreq mreq {};
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    mreq.imr_interface = iface_addr;

    if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        perror("setsockopt IP_ADD_MEMBERSHIP");
        close(fd);
        return 1;
    }

    int rcvbuf = 4 * 1500;  // 10 packets
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));

    std::cout << "Listening on " << MULTICAST_GROUP << ":" << PORT << " using interface " << INTERFACE_IP << "\n";

#if RAV_APPLE
    constexpr auto min_packet_time = 125 * 1000;       // 125us
    constexpr auto max_packet_time = 4 * 1000 * 1000;  // 4ms
    if (!rav::set_thread_realtime(min_packet_time, max_packet_time, max_packet_time * 2)) {
        throw std::runtime_error("Failed to set thread realtime");
    }
#endif

    State state {};

    char buffer[BUFFER_SIZE];
    while (true) {
        int bytes_available = 0;
        if (ioctl(fd, FIONREAD, &bytes_available) == -1) {
            perror("ioctl(FIONREAD)");
            return -1;  // Error
        }

        if (bytes_available > 0) {
            sockaddr_in src_addr {};
            socklen_t addrlen = sizeof(src_addr);
            const ssize_t len =
                recvfrom(fd, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&src_addr), &addrlen);
            if (len < 0) {
                perror("recvfrom");
                break;
            }

            handle_received_packet(state);

            if (state.count >= NUM_PACKETS) {
                break;
            }
        }

        std::this_thread::yield();
    }

    fmt::println("Stats for io_context.poll(): {}", to_string(state));

    close(fd);
    return 0;
}

#else

int main() {
    std::cerr << "This tool is only available on POSIX systems.\n";
    return -1;
}

#endif
