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

#include "../rtcp_packet_view.hpp"
#include "../rtp_packet_view.hpp"
#include "rtp_session.hpp"
#include "ravennakit/core/util/subscriber_list.hpp"
#include "ravennakit/core/net/sockets/extended_udp_socket.hpp"
#include "ravennakit/core/net/sockets/udp_receiver.hpp"

#include <boost/asio.hpp>
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
        const boost::asio::ip::udp::endpoint& src_endpoint;
        const boost::asio::ip::udp::endpoint& dst_endpoint;
        uint64_t recv_time;  // Arbitrary monotonic in nanoseconds
    };

    struct RtcpPacketEvent {
        const rtcp::PacketView& packet;
        const Session& session;
        const boost::asio::ip::udp::endpoint& src_endpoint;
        const boost::asio::ip::udp::endpoint& dst_endpoint;
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
     * @param udp_receiver The UDP receiver to use.
     */
    explicit Receiver(UdpReceiver& udp_receiver);

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    Receiver(Receiver&&) = delete;
    Receiver& operator=(Receiver&&) = delete;

    /**
     * Adds a subscriber to the receiver.
     * @param subscriber The subscriber to add
     * @param session The session to subscribe to.
     * @param interface_address The interface address to receive rtp packets on.
     * @return true if the subscriber was added, or false if it was already in the list.
     */
    bool subscribe(Subscriber* subscriber, const Session& session, const boost::asio::ip::address_v4& interface_address);

    /**
     * Removes a subscriber from all sessions of the receiver.
     * @param subscriber The subscriber to remove.
     * @return true if the subscriber was removed, or false if it wasn't found.
     */
    bool unsubscribe(const Subscriber* subscriber);

  private:
    class SessionContext: UdpReceiver::Subscriber {
      public:
        explicit SessionContext(UdpReceiver& udp_receiver, Session session, boost::asio::ip::address_v4 interface_address);
        ~SessionContext() override;

        [[nodiscard]] bool add_subscriber(Receiver::Subscriber* subscriber);
        [[nodiscard]] bool remove_subscriber(const Receiver::Subscriber* subscriber);

        [[nodiscard]] size_t get_subscriber_count() const;

        [[nodiscard]] const Session& get_session() const;
        [[nodiscard]] const boost::asio::ip::address_v4& interface_address() const;

        // UdpReceiver::Subscriber overrides
        void on_receive(const ExtendedUdpSocket::RecvEvent& event) override;

      private:
        UdpReceiver& udp_receiver_;
        Session session_;
        boost::asio::ip::address_v4 interface_address_;
        std::vector<uint32_t> synchronization_sources_;
        SubscriberList<Receiver::Subscriber> subscribers_;

        void handle_incoming_rtp_data(const ExtendedUdpSocket::RecvEvent& event);
        void subscribe_to_udp_receiver(const boost::asio::ip::address_v4& interface_address);
    };

    UdpReceiver& udp_receiver_;
    std::vector<std::unique_ptr<SessionContext>> sessions_contexts_;

    [[nodiscard]] SessionContext*
    find_session_context(const Session& session, const boost::asio::ip::address_v4& interface_address) const;
    SessionContext* create_new_session_context(const Session& session, const boost::asio::ip::address_v4& interface_address);
    SessionContext*
    find_or_create_session_context(const Session& session, const boost::asio::ip::address_v4& interface_address);
};

}  // namespace rav::rtp
