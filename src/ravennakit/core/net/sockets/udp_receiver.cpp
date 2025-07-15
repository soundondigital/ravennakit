/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/net/sockets/udp_receiver.hpp"

rav::UdpReceiver::UdpReceiver(boost::asio::io_context& io_context) : io_context_(io_context) {}

bool rav::UdpReceiver::subscribe(
    Subscriber* subscriber, const boost::asio::ip::address_v4& interface_address, const uint16_t port
) {
    RAV_ASSERT(subscriber != nullptr, "Subscriber should not be nullptr");
    RAV_ASSERT(!interface_address.is_multicast(), "Interface address should not be multicast");

    if (interface_address.is_unspecified()) {
        RAV_WARNING("Unspecified interface address is not allowed");
        // The reason to warn about this is that the 'any' socket bind will only work as long as there are no
        // bindings to a specific interface. At least on macOS bindings to a specific interface get precedence over
        // the 'any' socket for unicast traffic. This is not the case for multicast traffic, which requires the
        // 'any' socket anyway (on macOS).
    }

    auto& context = find_or_create_socket_context({interface_address, port});
    return context.add_subscriber(subscriber);
}

bool rav::UdpReceiver::subscribe(
    Subscriber* subscriber, const boost::asio::ip::address_v4& multicast_address,
    const boost::asio::ip::address_v4& interface_address, const uint16_t port
) {
    RAV_ASSERT(subscriber != nullptr, "Subscriber should not be nullptr");
    RAV_ASSERT(multicast_address.is_multicast(), "Multicast address is not multicast");
    RAV_ASSERT(!interface_address.is_unspecified(), "Interface address should not be unspecified");
    RAV_ASSERT(!interface_address.is_multicast(), "Interface address should not be multicast");

#if RAV_WINDOWS
    auto& context = find_or_create_socket_context({interface_address, port});
#else
    auto& context = find_or_create_socket_context({boost::asio::ip::address_v4::any(), port});
#endif

    return context.add_subscriber(subscriber, {multicast_address, interface_address});
}

void rav::UdpReceiver::unsubscribe(const Subscriber* subscriber) {
    for (const auto& socket : sockets_) {
        std::ignore = socket->remove_subscriber(subscriber);
    }
    auto count = cleanup_empty_sockets();
    if (count > 0) {
        RAV_TRACE("Removed {} socket(s)", count);
    }
}

rav::UdpReceiver::SocketContext::SocketContext(boost::asio::io_context& io_context, boost::asio::ip::udp::endpoint local_endpoint) :
    local_endpoint_(std::move(local_endpoint)), socket_(io_context, local_endpoint_) {
    RAV_ASSERT(!local_endpoint_.address().is_multicast(), "Interface address should not be a multicast address");
    RAV_ASSERT(local_endpoint_.port() != 0, "Port should not be 0");
    RAV_ASSERT(local_endpoint_.address().is_v4(), "Only IPv4 is supported");
    socket_.start([this](const ExtendedUdpSocket::RecvEvent& event) {
        handle_recv_event(event);
    });
}

bool rav::UdpReceiver::SocketContext::add_subscriber(Subscriber* subscriber, const MulticastGroup& multicast_group) {
    RAV_ASSERT(multicast_group.multicast_address.is_multicast(), "Multicast address is not multicast");
    RAV_ASSERT(!multicast_group.interface_address.is_unspecified(), "Interface address should not be unspecified");
    RAV_ASSERT(!multicast_group.interface_address.is_multicast(), "Interface address should not be multicast");

    const bool join_group = !has_multicast_group_subscriber(multicast_group);

    if (!subscribers_.add(subscriber, multicast_group)) {
        RAV_ERROR("Failed to add subscriber (already subscribed?)");
        return false;
    }

    if (join_group) {
        if (const auto ec =
                socket_.join_multicast_group(multicast_group.multicast_address, multicast_group.interface_address)) {
            RAV_ERROR("Failed to join multicast group: {}", ec.message());
            return false;
        }
    }

    return true;
}

bool rav::UdpReceiver::SocketContext::add_subscriber(Subscriber* subscriber) {
    if (!subscribers_.add(subscriber, {})) {
        RAV_ERROR("Failed to add subscriber (already subscribed?)");
        return false;
    }
    // This subscriber is only interested in unicast traffic so nothing to do here.
    return true;
}

bool rav::UdpReceiver::SocketContext::remove_subscriber(const Subscriber* subscriber) {
    const auto addr = subscribers_.remove(subscriber);
    if (!addr.has_value()) {
        return false;
    }

    if (addr->multicast_address.is_multicast()) {
        RAV_ASSERT(!addr->interface_address.is_unspecified(), "Interface address should not be unspecified");
        RAV_ASSERT(!addr->interface_address.is_multicast(), "Interface address should not be multicast");
        // Check if this was the last subscriber for this multicast group and if so, leave the group
        if (!has_multicast_group_subscriber(*addr)) {
            if (const auto ec = socket_.leave_multicast_group(addr->multicast_address, addr->interface_address)) {
                RAV_ERROR("Failed to leave multicast group: {}", ec.message());
                return true;  // Still return true even if leaving the group failed,  because the subscriber was removed
                              // successfully.
            }
        }
    }

    return true;
}

const boost::asio::ip::udp::endpoint& rav::UdpReceiver::SocketContext::local_endpoint() const {
    return local_endpoint_;
}

bool rav::UdpReceiver::SocketContext::empty() const {
    return subscribers_.empty();
}

bool rav::UdpReceiver::SocketContext::has_multicast_group_subscriber(const MulticastGroup& multicast_group) const {
    for (auto& [s, group] : subscribers_) {
        if (group == multicast_group) {
            return true;
        }
    }
    return false;
}

void rav::UdpReceiver::SocketContext::handle_recv_event(const ExtendedUdpSocket::RecvEvent& event) {
    const auto dst_addr = event.dst_endpoint.address();
    if (dst_addr.is_multicast()) {
        for (const auto& [s, m] : subscribers_) {
            if (dst_addr == m.multicast_address) {
                s->on_receive(event);
            }
        }
        return;
    }
    // Unicast event
    for (const auto& [s, m] : subscribers_) {
        // Only forward to unicast subscribers
        if (m.multicast_address.is_unspecified()) {
            s->on_receive(event);
        }
    }
}

rav::UdpReceiver::SocketContext* rav::UdpReceiver::find_socket_context(const boost::asio::ip::udp::endpoint& endpoint) const {
    for (auto& ctx : sockets_) {
        if (ctx->local_endpoint() == endpoint) {
            return ctx.get();
        }
    }
    return nullptr;
}

rav::UdpReceiver::SocketContext&
rav::UdpReceiver::find_or_create_socket_context(const boost::asio::ip::udp::endpoint& endpoint) {
    auto* context = find_socket_context(endpoint);
    if (context != nullptr) {
        return *context;
    }
    return *sockets_.emplace_back(std::make_unique<SocketContext>(io_context_, endpoint)).get();
}

size_t rav::UdpReceiver::cleanup_empty_sockets() {
    // Remove all sockets that are empty and return the number of removed sockets.
    size_t count = 0;
    for (auto it = sockets_.begin(); it != sockets_.end();) {
        if ((*it)->empty()) {
            it = sockets_.erase(it);
            count++;
        } else {
            ++it;
        }
    }
    return count;
}
