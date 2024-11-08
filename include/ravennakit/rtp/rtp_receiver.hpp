/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include "rtcp_packet_view.hpp"
#include "rtp_packet_view.hpp"
#include "detail/udp_sender_receiver.hpp"
#include "ravennakit/core/events.hpp"
#include "ravennakit/core/linked_node.hpp"
#include "ravennakit/core/subscriber_list.hpp"
#include "ravennakit/sdp/media_description.hpp"

#include <asio.hpp>

namespace rav {

/**
 * A class which sets up sockets for receiver RTP and RTCP packets, and allows subscribing to the received packets.
 */
class rtp_receiver {
  public:
    struct rtp_packet_event {
        const rtp_packet_view& packet;
        const asio::ip::udp::endpoint& src_endpoint;
        const asio::ip::udp::endpoint& dst_endpoint;
    };

    struct rtcp_packet_event {
        const rtcp_packet_view& packet;
        const asio::ip::udp::endpoint& src_endpoint;
        const asio::ip::udp::endpoint& dst_endpoint;
    };

    /**
     * Baseclass for other classes that want to subscribe to receiving RTP and RTCP packets.
     * The class provides several facilities to filter the traffic.
     */
    class subscriber {
      public:
        subscriber() = default;
        virtual ~subscriber() = default;

        subscriber(const subscriber&) = delete;
        subscriber& operator=(const subscriber&) = delete;

        subscriber(subscriber&&) noexcept = delete;
        subscriber& operator=(subscriber&&) noexcept = delete;

        /**
         * Called when an RTP packet is received.
         * @param rtp_event The RTP packet event.
         */
        virtual void on([[maybe_unused]] const rtp_packet_event& rtp_event) {}

        /**
         * Called when an RTCP packet is received.
         * @param rtcp_event The RTCP packet event.
         */
        virtual void on([[maybe_unused]] const rtcp_packet_event& rtcp_event) {}
    };

    rtp_receiver() = delete;
    ~rtp_receiver();

    /**
     * Constructs a new RTP receiver using given loop.
     * @param io_context The io_context to use.
     */
    explicit rtp_receiver(asio::io_context& io_context);

    rtp_receiver(const rtp_receiver&) = delete;
    rtp_receiver& operator=(const rtp_receiver&) = delete;

    rtp_receiver(rtp_receiver&&) = delete;
    rtp_receiver& operator=(rtp_receiver&&) = delete;

    void start(
        const asio::ip::address& bind_addr = asio::ip::address_v6(), uint16_t rtp_port = 5004, uint16_t rtcp_port = 5005
    );
    void stop();

    void join_multicast_group(const asio::ip::address& multicast_address, const asio::ip::address& interface_address) const;

    void subscribe(subscriber& subscriber);
    void unsubscribe(subscriber& subscriber);

  private:
    struct subscriber_context {
        asio::ip::address connection_address;
    };

    asio::io_context& io_context_;
    std::shared_ptr<udp_sender_receiver> rtp_socket_;
    std::shared_ptr<udp_sender_receiver> rtcp_socket_;
    subscriber_list<subscriber> subscribers_;
};

}  // namespace rav
