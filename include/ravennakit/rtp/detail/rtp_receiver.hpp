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

#include "rtp_filter.hpp"
#include "../rtcp_packet_view.hpp"
#include "../rtp_packet_view.hpp"
#include "rtp_session.hpp"
#include "udp_sender_receiver.hpp"
#include "ravennakit/core/events.hpp"
#include "ravennakit/core/linked_node.hpp"
#include "ravennakit/core/subscriber_list.hpp"
#include "ravennakit/sdp/media_description.hpp"

#include <asio.hpp>
#include <utility>

namespace rav {

/**
 * A class which sets up sockets for receiver RTP and RTCP packets, and allows subscribing to the received packets.
 */
class rtp_receiver {
  public:
    struct rtp_packet_event {
        const rtp_packet_view& packet;
        const rtp_session& session;
        const asio::ip::udp::endpoint& src_endpoint;
    };

    struct rtcp_packet_event {
        const rtcp_packet_view& packet;
        const rtp_session& session;
        const asio::ip::udp::endpoint& src_endpoint;
    };

    struct configuration {
        asio::ip::address interface_address {};
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
        virtual void on_rtp_packet([[maybe_unused]] const rtp_packet_event& rtp_event) {}

        /**
         * Called when an RTCP packet is received.
         * @param rtcp_event The RTCP packet event.
         */
        virtual void on_rtcp_packet([[maybe_unused]] const rtcp_packet_event& rtcp_event) {}
    };

    rtp_receiver() = delete;
    ~rtp_receiver() = default;

    /**
     * Constructs a new RTP receiver using given loop.
     * @param io_context The io_context to use.
     * @param config The configuration to use.
     */
    explicit rtp_receiver(asio::io_context& io_context, configuration config);

    rtp_receiver(const rtp_receiver&) = delete;
    rtp_receiver& operator=(const rtp_receiver&) = delete;

    rtp_receiver(rtp_receiver&&) = delete;
    rtp_receiver& operator=(rtp_receiver&&) = delete;

    /**
     * Subscribes to the given session.
     * @param subscriber_to_add The subscriber to add.
     * @param session The session to subscribe to.
     * @param filter
     */
    void subscribe(subscriber& subscriber_to_add, const rtp_session& session, const rtp_filter& filter);

    /**
     * Unsubscribes given subscriber from all sessions it's subscribed to.
     * @param subscriber_to_remove The subscriber to remove.
     */
    void unsubscribe(const subscriber& subscriber_to_remove);

  private:
    struct subscriber_context {
        rtp_filter filter;
    };

    class stream_state {
      public:
        explicit stream_state(const uint32_t ssrc) : ssrc_(ssrc) {}

        [[nodiscard]] uint32_t ssrc() const {
            return ssrc_;
        }

      private:
        uint32_t ssrc_ {};
    };

    struct session_context {
        rtp_session session;
        std::vector<stream_state> stream_states;
        subscriber_list<subscriber, subscriber_context> subscribers;
        std::shared_ptr<udp_sender_receiver> rtp_sender_receiver;
        std::shared_ptr<udp_sender_receiver> rtcp_sender_receiver;
        subscription rtp_multicast_subscription;
        subscription rtcp_multicast_subscription;
    };

    asio::io_context& io_context_;
    configuration config_;
    std::vector<session_context> sessions_contexts_;

    session_context* find_session_context(const rtp_session& session);
    session_context* create_new_session_context(const rtp_session& session);
    session_context* find_or_create_session_context(const rtp_session& session);
    std::shared_ptr<udp_sender_receiver> find_rtp_sender_receiver(uint16_t port);
    std::shared_ptr<udp_sender_receiver> find_rtcp_sender_receiver(uint16_t port);

    void handle_incoming_rtp_data(const udp_sender_receiver::recv_event& event);
    void handle_incoming_rtcp_data(const udp_sender_receiver::recv_event& event);
};

}  // namespace rav
