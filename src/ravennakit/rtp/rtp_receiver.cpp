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
#include "ravennakit/rtp/rtp_receiver.hpp"

#include "ravennakit/core/subscriber_list.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/core/tracy.hpp"
#include "ravennakit/core/subscription.hpp"

#include <fmt/core.h>
#include <tl/expected.hpp>

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

rav::rtp_receiver::rtp_receiver(asio::io_context& io_context) : io_context_(io_context) {}

void rav::rtp_receiver::subscribe(subscriber& subscriber_to_add, const rtp_session& session) {
    auto* context = find_or_create_session_context(session);

    if (context == nullptr) {
        RAV_WARNING("Failed to find or create new session");
        return;
    }

    RAV_ASSERT(context != nullptr, "Expecting valid session at this point");

    if (!context->subscribers.add(&subscriber_to_add)) {
        // Note: this can never happen if the session was created, hence it's find to not clean up the session here.
        RAV_WARNING("Already subscribed to session");
    }
}

void rav::rtp_receiver::unsubscribe(subscriber& subscriber_to_add) {
    for (auto it = sessions_contexts_.begin(); it != sessions_contexts_.end();) {
        if (it->subscribers.remove(&subscriber_to_add) && it->subscribers.empty()) {
            it = sessions_contexts_.erase(it);  // Remove the session
        } else {
            ++it;
        }
    }
}

rav::rtp_receiver::session_context* rav::rtp_receiver::find_session_context(const rtp_session& session) {
    for (auto& context : sessions_contexts_) {
        if (context.session == session) {
            return &context;
        }
    }
    return nullptr;
}

rav::rtp_receiver::session_context* rav::rtp_receiver::create_new_session_context(const rtp_session& session) {
    session_context new_session;
    new_session.session = session;
    new_session.rtp_sender_receiver = find_rtp_sender_receiver(session.rtp_port);
    new_session.rtcp_sender_receiver = find_rtcp_sender_receiver(session.rtcp_port);

    const auto bind_addr = asio::ip::address_v4();

    if (new_session.rtp_sender_receiver == nullptr) {
        new_session.rtp_sender_receiver =
            std::make_shared<udp_sender_receiver>(io_context_, bind_addr, session.rtp_port);
        // Capturing this is valid because rtp_receiver will stop the udp_sender_receiver before it goes out of scope.
        new_session.rtp_sender_receiver->start([this](const udp_sender_receiver::recv_event& event) {
            handle_incoming_rtp_data(event);
        });
        if (session.connection_address.is_multicast()) {
            new_session.rtp_multicast_subscription =
                new_session.rtp_sender_receiver->join_multicast_group(session.connection_address, bind_addr);
        }
    }

    if (new_session.rtcp_sender_receiver == nullptr) {
        new_session.rtcp_sender_receiver =
            std::make_shared<udp_sender_receiver>(io_context_, bind_addr, session.rtcp_port);
        // Capturing this is valid because rtp_receiver will stop the udp_sender_receiver before it goes out of scope.
        new_session.rtcp_sender_receiver->start([this](const udp_sender_receiver::recv_event& event) {
            handle_incoming_rtcp_data(event);
        });
        if (session.connection_address.is_multicast()) {
            new_session.rtcp_multicast_subscription =
                new_session.rtcp_sender_receiver->join_multicast_group(session.connection_address, bind_addr);
        }
    }

    sessions_contexts_.emplace_back(std::move(new_session));
    return &sessions_contexts_.back();
}

rav::rtp_receiver::session_context* rav::rtp_receiver::find_or_create_session_context(const rtp_session& session) {
    auto context = find_session_context(session);
    if (context == nullptr) {
        context = create_new_session_context(session);
    }
    return context;
}

std::shared_ptr<rav::udp_sender_receiver> rav::rtp_receiver::find_rtp_sender_receiver(const uint16_t port) {
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

std::shared_ptr<rav::udp_sender_receiver> rav::rtp_receiver::find_rtcp_sender_receiver(const uint16_t port) {
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

void rav::rtp_receiver::handle_incoming_rtp_data(const udp_sender_receiver::recv_event& event) {
    const rtp_packet_view packet(event.data, event.size);
    if (!packet.validate()) {
        RAV_WARNING("Invalid RTP packet received");
        return;
    }
    const rtp_packet_event rtp_event {packet, event.src_endpoint, event.dst_endpoint};
    RAV_INFO(
        "{} from {}:{} to {}:{}", packet.to_string(), rtp_event.src_endpoint.address().to_string(),
        rtp_event.src_endpoint.port(), rtp_event.dst_endpoint.address().to_string(), rtp_event.dst_endpoint.port()
    );

    // TODO: Process the packet
}

void rav::rtp_receiver::handle_incoming_rtcp_data(const udp_sender_receiver::recv_event& event) {
    const rtcp_packet_view packet(event.data, event.size);
    if (!packet.validate()) {
        RAV_WARNING("Invalid RTCP packet received");
        return;
    }
    const rtcp_packet_event rtcp_event {packet, event.src_endpoint, event.dst_endpoint};
    RAV_INFO(
        "{} from {}:{} to {}:{}", packet.to_string(), rtcp_event.src_endpoint.address().to_string(),
        rtcp_event.src_endpoint.port(), rtcp_event.dst_endpoint.address().to_string(), rtcp_event.dst_endpoint.port()
    );

    // TODO: Process the packet
}
