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
#include "detail/rtp_endpoint.hpp"
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
    using rtp_packet_event = rtp_endpoint::rtp_packet_event;
    using rtcp_packet_event = rtp_endpoint::rtcp_packet_event;

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

      protected:
        /**
         * Subscribes to the given RTP receiver.
         * @param receiver The receiver to subscribe to.
         * @param address
         * @param port
         */
        void subscribe_to_rtp_session(rtp_receiver& receiver, const asio::ip::address& address, uint16_t port);

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

  private:
    class session : public rtp_endpoint::handler {
      public:
        session(asio::io_context& io_context, asio::ip::udp::endpoint endpoint);
        ~session() override;

        void on(const rtp_packet_event& rtp_event) override;
        void on(const rtcp_packet_event& rtcp_event) override;

        [[nodiscard]] asio::ip::udp::endpoint connection_endpoint() const;

      private:
        std::shared_ptr<rtp_endpoint> rtp_endpoint_;
        linked_node<std::pair<subscriber*, rtp_receiver*>> subscriber_nodes_;
    };

    asio::io_context& io_context_;
    std::vector<std::unique_ptr<session>> sessions_;
};

}  // namespace rav
