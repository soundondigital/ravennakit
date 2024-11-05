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
#include "ravennakit/core/events.hpp"
#include "ravennakit/core/linked_node.hpp"
#include "ravennakit/core/subscriber_list.hpp"

#include <asio.hpp>

namespace rav {

/**
 * A class which sets up sockets for receiver RTP and RTCP packets, and allows subscribing to the received packets.
 */
class rtp_receiver {
  public:
    struct rtp_packet_event {
        const rtp_packet_view& packet;
    };

    struct rtcp_packet_event {
        const rtcp_packet_view& packet;
    };

    /**
     * Baseclass for other classes that want to subscribe to receiving RTP and RTCP packets.
     * The class provides several facilities to filter the traffic.
     */
    class subscriber {
      public:
        virtual ~subscriber() = default;

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

        /**
         * Subscribes to the given RTP receiver.
         * @param receiver The receiver to subscribe to.
         */
        void subscribe(rtp_receiver& receiver);

      private:
        linked_node<std::pair<subscriber*, rtp_receiver*>> node_;
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

    /**
     * Binds 2 UDP sockets to the given address and port. One for receiving RTP packets and one for receiving RTCP
     * packets. The sockets will be bound to the given address and port. The port number will be used for RTP and port
     * number + 1 will be used for RTCP.
     * @param address The address to bind to.
     * @param port The port to bind to. Default is 5004.
     * @return A result indicating success or failure.
     */
    void bind(const std::string& address, uint16_t port = 5004) const;

    /**
     * Sets the multicast membership for the given multicast address and interface address.
     * @param multicast_address The multicast address to join or leave.
     * @param interface_address The interface address to use.
     * @return A result indicating success or failure.
     */
    void join_multicast_group(const std::string& multicast_address, const std::string& interface_address) const;

    /**
     * @return Starts receiving datagrams on the bound sockets.
     */
    void start() const;

    /**
     * Stops receiving datagrams on the bound sockets.
     */
    void stop() const;

  private:
    class impl;
    std::shared_ptr<impl> impl_;
    linked_node<std::pair<subscriber*, rtp_receiver*>> subscriber_nodes_;

};

}  // namespace rav
