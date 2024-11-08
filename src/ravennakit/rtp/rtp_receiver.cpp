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

rav::rtp_receiver::~rtp_receiver() {
    stop();
}

void rav::rtp_receiver::start(const asio::ip::address& bind_addr, uint16_t rtp_port, uint16_t rtcp_port) {
    // While RFC 3550 recommends, rather than requires, a specific pairing pattern for RTP and RTCP ports,
    // we enforce a stricter limitation to ensure compatibility when managing multiple `rtp_receiver` instances.
    // Allowing arbitrary port assignments could result in conflicts, such as two pairs like 5004/5005 and 5004/5006,
    // where both receivers would attempt to listen on the same RTP port — a scenario incompatible with most platforms.
    // By enforcing this stricter port pairing, we avoid potential conflicts, ensure predictable behavior,
    // and facilitate the effective reuse of `rtp_receiver` instances.
    // https://datatracker.ietf.org/doc/html/rfc3550#section-11
    RAV_ASSERT(rtp_port % 2 == 0, "RTP port must be even");
    RAV_ASSERT(rtp_port + 1 == rtcp_port, "RTCP port must be one higher than RTP port");

    if (rtp_socket_ || rtcp_socket_) {
        RAV_WARNING("RTP receiver already running");
        return;
    }

    auto rtp_socket = udp_sender_receiver::make(io_context_, bind_addr, rtp_port);
    auto rtcp_socket = udp_sender_receiver::make(io_context_, bind_addr, rtcp_port);

    rtp_socket->start([this](
                          const uint8_t* data, const size_t size, const asio::ip::udp::endpoint& src,
                          const asio::ip::udp::endpoint& dst
                      ) {
        const rtp_packet_view packet(data, size);
        if (!packet.validate()) {
            RAV_WARNING("Invalid RTP packet received");
            return;
        }
        const rtp_packet_event event {packet, src, dst};
        RAV_INFO(
            "{} from {}:{} to {}:{}", packet.to_string(), event.src_endpoint.address().to_string(),
            event.src_endpoint.port(), event.dst_endpoint.address().to_string(), event.dst_endpoint.port()
        );

        // TODO: Process the packet
    });

    rtcp_socket->start([this](
                           const uint8_t* data, const size_t size, const asio::ip::udp::endpoint& src,
                           const asio::ip::udp::endpoint& dst
                       ) {
        const rtcp_packet_view packet(data, size);
        if (!packet.validate()) {
            RAV_WARNING("Invalid RTCP packet received");
            return;
        }
        const rtcp_packet_event event {packet, src, dst};
        RAV_INFO(
            "{} from {}:{} to {}:{}", packet.to_string(), event.src_endpoint.address().to_string(),
            event.src_endpoint.port(), event.dst_endpoint.address().to_string(), event.dst_endpoint.port()
        );
        // TODO: Process the packet
    });

    RAV_TRACE(
        "RTP Receiver started. RTP on {}:{}, RTCP on {}:{}", bind_addr.to_string(), rtp_port, bind_addr.to_string(),
        rtcp_port
    );

    rtp_socket_ = std::move(rtp_socket);
    rtcp_socket_ = std::move(rtcp_socket);
}

void rav::rtp_receiver::stop() {
    if (!rtp_socket_ && !rtcp_socket_) {
        return;  // Nothing to do here
    }

    if (rtp_socket_) {
        rtp_socket_->stop();
        rtp_socket_.reset();
    }

    if (rtcp_socket_) {
        rtcp_socket_->stop();
        rtcp_socket_.reset();
    }

    RAV_TRACE("RTP Receiver stopped.");
}

void rav::rtp_receiver::join_multicast_group(
    const asio::ip::address& multicast_address, const asio::ip::address& interface_address
) const {
    if (rtp_socket_ == nullptr || rtcp_socket_ == nullptr) {
        RAV_ERROR("RTP receiver is not running");
        return;
    }
    rtp_socket_->join_multicast_group(multicast_address, interface_address);
    rtcp_socket_->join_multicast_group(multicast_address, interface_address);
}

void rav::rtp_receiver::subscribe(subscriber& subscriber) {
    subscribers_.add(&subscriber);
}

void rav::rtp_receiver::unsubscribe(subscriber& subscriber) {
    subscribers_.remove(&subscriber);
}
