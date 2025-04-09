/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/log.hpp"
#include "ravennakit/rtp/rtcp_packet_view.hpp"
#include "ravennakit/rtp/detail/rtp_receiver.hpp"

#include "ravennakit/core/subscriber_list.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/core/tracy.hpp"
#include "ravennakit/core/subscription.hpp"

#include <fmt/core.h>
#include "ravennakit/core/expected.hpp"

#include <utility>

#if RAV_APPLE
    #define IP_RECVDSTADDR_PKTINFO IP_RECVDSTADDR
#else
    #define IP_RECVDSTADDR_PKTINFO IP_PKTINFO
#endif

#if RAV_WINDOWS
typedef BOOL(PASCAL* LPFN_WSARECVMSG)(
    SOCKET s, LPWSAMSG lpMsg, LPDWORD lpNumberOfBytesRecvd, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);
#endif

rav::rtp::Receiver::Receiver(asio::io_context& io_context) : io_context_(io_context) {}

asio::io_context& rav::rtp::Receiver::get_io_context() const {
    return io_context_;
}

bool rav::rtp::Receiver::subscribe(Subscriber* subscriber_to_add, const Session& session, const Filter& filter) {
    auto* context = find_or_create_session_context(session);

    if (context == nullptr) {
        RAV_WARNING("Failed to find or create new session context");
        return false;
    }

    RAV_ASSERT(context != nullptr, "Expecting valid session at this point");

    return context->subscribers.add_or_update_context(subscriber_to_add, SubscriberContext {filter});
}

bool rav::rtp::Receiver::unsubscribe(const Subscriber* subscriber_to_remove) {
    size_t count = 0;
    for (auto it = sessions_contexts_.begin(); it != sessions_contexts_.end();) {
        if (it->subscribers.remove(subscriber_to_remove)) {
            count++;
        }
        if (it->subscribers.empty()) {
            it = sessions_contexts_.erase(it);
        } else {
            ++it;
        }
    }
    return count > 0;
}

void rav::rtp::Receiver::set_interface(const asio::ip::address& interface_address) {
    config_.interface_address = interface_address;
    for (auto& session : sessions_contexts_) {
        if (!interface_address.is_unspecified() && session.session.connection_address.is_multicast()) {
            session.rtp_multicast_subscription = session.rtp_sender_receiver->join_multicast_group(
                session.session.connection_address, interface_address
            );
            session.rtcp_multicast_subscription = session.rtcp_sender_receiver->join_multicast_group(
                session.session.connection_address, interface_address
            );
        } else {
            session.rtp_multicast_subscription.reset();
            session.rtcp_multicast_subscription.reset();
        }
    }
}

rav::rtp::Receiver::SessionContext* rav::rtp::Receiver::find_session_context(const Session& session) {
    for (auto& context : sessions_contexts_) {
        if (context.session == session) {
            return &context;
        }
    }
    return nullptr;
}

rav::rtp::Receiver::SessionContext* rav::rtp::Receiver::create_new_session_context(const Session& session) {
    SessionContext new_session;
    new_session.session = session;
    new_session.rtp_sender_receiver = find_rtp_sender_receiver(session.rtp_port);
    new_session.rtcp_sender_receiver = find_rtcp_sender_receiver(session.rtcp_port);

    // Note: we bind to the any address because the behaviour of macOS and Windows slightly differs. On macOS the bind
    // address functions as a filter (at least when joining a multicast group), while on Windows it's the actual address
    // to bind to. Secondly, binding to the multicast address would work on macOS, but not on Windows where this is an
    // invalid operation. To have a cross-platform solution we bind to the any address, which potentially results in
    // more traffic being received than we need, but since we're filtering on the endpoint anyway, this is not a
    // problem.
    // The ideal solution would be a platform-specific implementation, where on macOS we would use a single socket bound
    // to a specific interface for unicast and a separate sockets for each multicast address (bound to the multicast
    // address to filter). On Windows we would use a single socket for all traffic (unicast + multicast) but bound to a
    // specific interface (address).
    // To summarize: on macOS we cannot bind to a specific interface and receive both unicast and multicast traffic.

    // TODO: Disallow a port to be used in multiple sessions because when receiving RTP data we don't know which session
    // it belongs to.

    // For future refactor:
    // - Create a udp_sender_receiver_pool which manages udp_sender_receiver instances (weakly reference them). This
    // pool will hand out a single instance per interface address + port on Windows and separate instances on macos for
    // address + multicast addresses + unicast. This allows the socket to bind to a specific interface (see above).
    // - Make rtp_receiver work with a port pair (i.e. 5004/5004). Limit a port to only be used in a single port pair.
    // - Create a rtp_receiver_pool which manages rtp_receiver instances (weakly reference them). This pool will hand
    // out instances based on port pair. If one of the ports is already used in another instance, refuse.
    // - rtp_stream_receiver gets a rtp_receiver from the pool and holds on to it locally. This allows multiple streams
    // to use the same rtp_receiver instance.

    if (new_session.rtp_sender_receiver == nullptr) {
        new_session.rtp_sender_receiver =
            std::make_shared<UdpSenderReceiver>(io_context_, asio::ip::address_v4(), session.rtp_port);
        // Capturing this is valid because rtp_receiver will stop the udp_sender_receiver before it goes out of scope.
        new_session.rtp_sender_receiver->start([this](const UdpSenderReceiver::recv_event& event) {
            handle_incoming_rtp_data(event);
        });
    }

    if (new_session.rtcp_sender_receiver == nullptr) {
        new_session.rtcp_sender_receiver =
            std::make_shared<UdpSenderReceiver>(io_context_, asio::ip::address_v4(), session.rtcp_port);
        // Capturing this is valid because rtp_receiver will stop the udp_sender_receiver before it goes out of scope.
        new_session.rtcp_sender_receiver->start([this](const UdpSenderReceiver::recv_event& event) {
            handle_incoming_rtcp_data(event);
        });
    }

    if (session.connection_address.is_multicast()) {
        new_session.rtp_multicast_subscription = new_session.rtp_sender_receiver->join_multicast_group(
            session.connection_address, config_.interface_address
        );
        new_session.rtcp_multicast_subscription = new_session.rtcp_sender_receiver->join_multicast_group(
            session.connection_address, config_.interface_address
        );
    }

    sessions_contexts_.emplace_back(std::move(new_session));
    RAV_TRACE("New RTP session context created for: {}", session.to_string());
    return &sessions_contexts_.back();
}

rav::rtp::Receiver::SessionContext* rav::rtp::Receiver::find_or_create_session_context(const Session& session) {
    auto context = find_session_context(session);
    if (context == nullptr) {
        context = create_new_session_context(session);
    }
    return context;
}

std::shared_ptr<rav::rtp::UdpSenderReceiver> rav::rtp::Receiver::find_rtp_sender_receiver(const uint16_t port) {
    for (auto& context : sessions_contexts_) {
        if (context.session.rtcp_port == port) {
            RAV_WARNING("RTCP port found instead of RTP port. This is a network administration error.");
            return nullptr;
        }
        if (context.session.rtp_port == port) {
            return context.rtp_sender_receiver;
        }
    }
    return {};
}

std::shared_ptr<rav::rtp::UdpSenderReceiver> rav::rtp::Receiver::find_rtcp_sender_receiver(const uint16_t port) {
    for (auto& session : sessions_contexts_) {
        if (session.session.rtp_port == port) {
            RAV_WARNING("RTP port found instead of RTCP port. This is a network administration error.");
            return nullptr;
        }
        if (session.session.rtcp_port == port) {
            return session.rtcp_sender_receiver;
        }
    }
    return {};
}

void rav::rtp::Receiver::handle_incoming_rtp_data(const UdpSenderReceiver::recv_event& event) {
    TRACY_ZONE_SCOPED;

    const PacketView packet(event.data, event.size);
    if (!packet.validate()) {
        RAV_WARNING("Invalid RTP packet received");
        return;
    }

    for (auto& context : sessions_contexts_) {
        if (context.session.connection_address == event.dst_endpoint.address()
            && context.session.rtp_port == event.dst_endpoint.port()) {
            const RtpPacketEvent rtp_event {packet, context.session, event.src_endpoint, event.recv_time};

            bool did_find_stream = false;

            for (auto& state : context.stream_states) {
                if (state.ssrc() == packet.ssrc()) {
                    did_find_stream = true;
                }
            }

            if (!did_find_stream) {
                auto& it = context.stream_states.emplace_back(packet.ssrc());
                RAV_TRACE(
                    "Added new stream with SSRC {} from {}:{}", it.ssrc(), event.src_endpoint.address().to_string(),
                    event.src_endpoint.port()
                );
            }

            for (auto& [sub, ctx] : context.subscribers) {
                if (ctx.filter.is_valid_source(event.dst_endpoint.address(), event.src_endpoint.address())) {
                    sub->on_rtp_packet(rtp_event);
                }
            }
        }
    }
}

void rav::rtp::Receiver::handle_incoming_rtcp_data(const UdpSenderReceiver::recv_event& event) {
    TRACY_ZONE_SCOPED;

    const rtcp::PacketView packet(event.data, event.size);
    if (!packet.validate()) {
        RAV_WARNING("Invalid RTCP packet received");
        return;
    }
    // const rtcp_packet_event rtcp_event {packet, event.src_endpoint, event.dst_endpoint};

    // TODO: Process the packet
}
