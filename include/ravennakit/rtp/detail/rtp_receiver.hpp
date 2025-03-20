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
#include "ravennakit/sdp/sdp_media_description.hpp"

#include <asio.hpp>
#include <utility>

namespace rav::rtp {

/**
 * A class which sets up sockets for receiver RTP and RTCP packets, and allows subscribing to the received packets.
 */
class Receiver {
  public:
    struct RtpPacketEvent {
        const PacketView& packet;
        const Session& session;
        const asio::ip::udp::endpoint& src_endpoint;
        uint64_t recv_time;  // Monotonically increasing time in nanoseconds with arbitrary starting point.
    };

    struct RtcpPacketEvent {
        const rtcp::PacketView& packet;
        const Session& session;
        const asio::ip::udp::endpoint& src_endpoint;
    };

    struct Configuration {
        asio::ip::address interface_address {};
    };

    /**
     * Baseclass for other classes that want to subscribe to receiving RTP and RTCP packets.
     * The class provides several facilities to filter the traffic.
     */
    class Subscriber {
      public:
        Subscriber() = default;
        virtual ~Subscriber() = default;

        Subscriber(const Subscriber&) = delete;
        Subscriber& operator=(const Subscriber&) = delete;

        Subscriber(Subscriber&&) noexcept = delete;
        Subscriber& operator=(Subscriber&&) noexcept = delete;

        /**
         * Called when an RTP packet is received.
         * @param rtp_event The RTP packet event.
         */
        virtual void on_rtp_packet([[maybe_unused]] const RtpPacketEvent& rtp_event) {}

        /**
         * Called when an RTCP packet is received.
         * @param rtcp_event The RTCP packet event.
         */
        virtual void on_rtcp_packet([[maybe_unused]] const RtcpPacketEvent& rtcp_event) {}
    };

    Receiver() = delete;
    ~Receiver() = default;

    /**
     * Constructs a new RTP receiver using given loop.
     * @param io_context The io_context to use.
     * @param config The configuration to use.
     */
    explicit Receiver(asio::io_context& io_context, Configuration config);

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    Receiver(Receiver&&) = delete;
    Receiver& operator=(Receiver&&) = delete;

    /**
     * @return The io_context associated with this receiver.
     */
    [[nodiscard]] asio::io_context& get_io_context() const;

    /**
     * Adds a subscriber to the receiver.
     * @param subscriber_to_add The subscriber to add
     * @param session The session to subscribe to.
     * @param filter The filter to apply to the session.
     * @return true if the subscriber was added, or false if it was already in the list.
     */
    bool subscribe(Subscriber* subscriber_to_add, const Session& session, const Filter& filter);

    /**
     * Removes a subscriber from all sessions of the receiver.
     * @param subscriber_to_remove The subscriber to remove.
     * @return true if the subscriber was removed, or false if it wasn't found.
     */
    bool unsubscribe(const Subscriber* subscriber_to_remove);

  private:
    struct SubscriberContext {
        Filter filter;
    };

    class StreamState {
      public:
        explicit StreamState(const uint32_t ssrc) : ssrc_(ssrc) {}

        [[nodiscard]] uint32_t ssrc() const {
            return ssrc_;
        }

      private:
        uint32_t ssrc_ {};
    };

    struct SessionContext {
        Session session;
        std::vector<StreamState> stream_states;
        subscriber_list<Subscriber, SubscriberContext> subscribers;
        std::shared_ptr<UdpSenderReceiver> rtp_sender_receiver;
        std::shared_ptr<UdpSenderReceiver> rtcp_sender_receiver;
        subscription rtp_multicast_subscription;
        subscription rtcp_multicast_subscription;
    };

    asio::io_context& io_context_;
    Configuration config_;
    std::vector<SessionContext> sessions_contexts_;

    SessionContext* find_session_context(const Session& session);
    SessionContext* create_new_session_context(const Session& session);
    SessionContext* find_or_create_session_context(const Session& session);
    std::shared_ptr<UdpSenderReceiver> find_rtp_sender_receiver(uint16_t port);
    std::shared_ptr<UdpSenderReceiver> find_rtcp_sender_receiver(uint16_t port);

    void handle_incoming_rtp_data(const UdpSenderReceiver::recv_event& event);
    void handle_incoming_rtcp_data(const UdpSenderReceiver::recv_event& event);
};

}  // namespace rav
