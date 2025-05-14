/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/system.hpp"
#include "ravennakit/core/net/sockets/extended_udp_socket.hpp"
#include "ravennakit/core/net/sockets/udp_receiver.hpp"
#include "ravennakit/core/util/throttle.hpp"

namespace {

class MySubscriber: public rav::UdpReceiver::Subscriber {
  public:
    size_t on_receive_count = 0;
    rav::Throttle<void> throttle {std::chrono::seconds(1)};

    void on_receive(const rav::ExtendedUdpSocket::RecvEvent& event) override {
        on_receive_count++;
        if (event.dst_endpoint.address().is_multicast()) {
            if (throttle.update()) {
                RAV_TRACE(
                    "Multicast: {}:{} from {}:{} on {} (total count = {})", event.dst_endpoint.address().to_string(),
                    event.dst_endpoint.port(), event.src_endpoint.address().to_string(), event.src_endpoint.port(),
                    static_cast<void*>(this), on_receive_count
                );
            }
        } else {
            RAV_TRACE(
                "Unicast: {}:{} from {}:{} on {} (total count = {})", event.dst_endpoint.address().to_string(),
                event.dst_endpoint.port(), event.src_endpoint.address().to_string(), event.src_endpoint.port(),
                static_cast<void*>(this), on_receive_count
            );
        }
    }
};

}  // namespace

int main() {
    rav::set_log_level_from_env();
    rav::do_system_checks();

    const auto interface1 = boost::asio::ip::make_address_v4("192.168.15.53");
    boost::asio::io_context io_context;

    rav::UdpReceiver udp_receiver(io_context);

    MySubscriber subscriber1;
    MySubscriber subscriber2;

    // Subscribe to unicast traffic on interface 1
    if (!udp_receiver.subscribe(&subscriber1, interface1, 5004)) {
        throw std::runtime_error("Failed to subscribe to interface 1");
    }

    // Subscribe to multicast traffic on interface 1
    if (!udp_receiver.subscribe(&subscriber2, boost::asio::ip::make_address_v4("239.15.55.1"), interface1, 5004)) {
        throw std::runtime_error("Failed to subscribe to interface 2");
    }

    io_context.run_for(std::chrono::seconds(10));

    udp_receiver.unsubscribe(&subscriber1);
    udp_receiver.unsubscribe(&subscriber2);

    return 0;
}
