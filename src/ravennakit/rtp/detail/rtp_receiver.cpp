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

#include "ravennakit/core/util/subscriber_list.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/core/util/tracy.hpp"

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

rav::rtp::Receiver::Receiver(UdpReceiver& udp_receiver) : udp_receiver_(udp_receiver) {}

bool rav::rtp::Receiver::subscribe(
    Subscriber* subscriber, const Session& session, const boost::asio::ip::address_v4& interface_address
) {
    auto* context = find_or_create_session_context(session, interface_address);

    if (context == nullptr) {
        RAV_WARNING("Failed to find or create new session context");
        return false;
    }

    RAV_ASSERT(context != nullptr, "Expecting valid session at this point");

    return context->add_subscriber(subscriber);
}

bool rav::rtp::Receiver::unsubscribe(const Subscriber* subscriber) {
    size_t count = 0;
    for (auto it = sessions_contexts_.begin(); it != sessions_contexts_.end();) {
        if ((*it)->remove_subscriber(subscriber)) {
            count++;
        }
        if ((*it)->get_subscriber_count() == 0) {
            it = sessions_contexts_.erase(it);
        } else {
            ++it;
        }
    }
    return count > 0;
}

rav::rtp::Receiver::SessionContext::SessionContext(
    UdpReceiver& udp_receiver, Session session, boost::asio::ip::address_v4 interface_address
) :
    udp_receiver_(udp_receiver), session_(std::move(session)), interface_address_(std::move(interface_address)) {
    RAV_ASSERT(!session_.connection_address.is_unspecified(), "Connection address should not be unspecified");
    RAV_ASSERT(!interface_address.is_unspecified(), "Interface address should not be unspecified");
    RAV_ASSERT(!interface_address.is_multicast(), "Interface address should not be multicast");
    subscribe_to_udp_receiver(interface_address);
}

rav::rtp::Receiver::SessionContext::~SessionContext() {
    udp_receiver_.unsubscribe(this);
}

bool rav::rtp::Receiver::SessionContext::add_subscriber(Receiver::Subscriber* subscriber) {
    return subscribers_.add(subscriber);
}

bool rav::rtp::Receiver::SessionContext::remove_subscriber(const Receiver::Subscriber* subscriber) {
    return subscribers_.remove(subscriber);
}

size_t rav::rtp::Receiver::SessionContext::get_subscriber_count() const {
    return subscribers_.size();
}

const rav::rtp::Session& rav::rtp::Receiver::SessionContext::get_session() const {
    return session_;
}

const boost::asio::ip::address_v4& rav::rtp::Receiver::SessionContext::interface_address() const {
    return interface_address_;
}

void rav::rtp::Receiver::SessionContext::on_receive(const ExtendedUdpSocket::RecvEvent& event) {
    if (event.dst_endpoint.port() == session_.rtp_port) {
        handle_incoming_rtp_data(event);
    } else if (event.dst_endpoint.port() == session_.rtcp_port) {
        // TODO: Handle RTCP data
    } else {
        RAV_WARNING("Received data on unknown port: {}", event.dst_endpoint.port());
    }
}

void rav::rtp::Receiver::SessionContext::handle_incoming_rtp_data(const ExtendedUdpSocket::RecvEvent& event) {
    TRACY_ZONE_SCOPED;

    const PacketView packet(event.data, event.size);
    if (!packet.validate()) {
        RAV_WARNING("Invalid RTP packet received");
        return;
    }

    const RtpPacketEvent rtp_event {packet, session_, event.src_endpoint, event.dst_endpoint, event.recv_time};

    bool did_find_stream = false;

    for (const auto& ssrc : synchronization_sources_) {
        if (ssrc == packet.ssrc()) {
            did_find_stream = true;
        }
    }

    if (!did_find_stream) {
        auto ssrc = synchronization_sources_.emplace_back(packet.ssrc());
        RAV_TRACE(
            "Added new stream with SSRC {} from {}:{}", ssrc, event.src_endpoint.address().to_string(),
            event.src_endpoint.port()
        );
    }

    for (auto* s : subscribers_) {
        s->on_rtp_packet(rtp_event);
    }
}

void rav::rtp::Receiver::SessionContext::subscribe_to_udp_receiver(const boost::asio::ip::address_v4& interface_address) {
    if (session_.connection_address.is_multicast()) {
        if (!udp_receiver_.subscribe(this, session_.connection_address.to_v4(), interface_address, session_.rtp_port)) {
            RAV_ERROR("Failed to subscribe to multicast RTP session {}", session_.to_string());
        }
        if (!udp_receiver_.subscribe(
                this, session_.connection_address.to_v4(), interface_address, session_.rtcp_port
            )) {
            RAV_ERROR("Failed to subscribe to multicast RTP session {}", session_.to_string());
        }
    } else {
        if (!udp_receiver_.subscribe(this, interface_address, session_.rtp_port)) {
            RAV_ERROR("Failed to subscribe to unicast RTP session {}", session_.to_string());
        }
        if (!udp_receiver_.subscribe(this, interface_address, session_.rtcp_port)) {
            RAV_ERROR("Failed to subscribe to unicast RTP session {}", session_.to_string());
        }
    }
}

rav::rtp::Receiver::SessionContext*
rav::rtp::Receiver::find_session_context(const Session& session, const boost::asio::ip::address_v4& interface_address) const {
    for (const auto& context : sessions_contexts_) {
        if (context->get_session() == session && context->interface_address() == interface_address) {
            return context.get();
        }
    }
    return nullptr;
}

rav::rtp::Receiver::SessionContext*
rav::rtp::Receiver::create_new_session_context(const Session& session, const boost::asio::ip::address_v4& interface_address) {
    // TODO: Disallow a port to be used in multiple sessions because when receiving RTP data we don't know which session
    // it belongs to.

    auto new_session = std::make_unique<SessionContext>(udp_receiver_, session, interface_address);
    const auto& it = sessions_contexts_.emplace_back(std::move(new_session));
    RAV_TRACE("New RTP session context created for: {}", session.to_string());
    return it.get();
}

rav::rtp::Receiver::SessionContext* rav::rtp::Receiver::find_or_create_session_context(
    const Session& session, const boost::asio::ip::address_v4& interface_address
) {
    auto context = find_session_context(session, interface_address);
    if (context == nullptr) {
        context = create_new_session_context(session, interface_address);
    }
    return context;
}
