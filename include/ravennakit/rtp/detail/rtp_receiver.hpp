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
#include "rtp_ringbuffer.hpp"
#include "../rtcp_packet_view.hpp"
#include "../rtp_packet_view.hpp"
#include "rtp_session.hpp"
#include "ravennakit/aes67/aes67_constants.hpp"
#include "ravennakit/core/util/subscriber_list.hpp"
#include "ravennakit/core/net/sockets/extended_udp_socket.hpp"
#include "ravennakit/core/net/sockets/udp_receiver.hpp"
#include "ravennakit/core/util/id.hpp"

#include <boost/asio.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/lockfree/policies.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include <utility>

namespace rav::rtp {

struct Receiver3 {
    static constexpr auto k_max_num_redundant_streams = 16;
    static constexpr auto k_max_num_rtp_sessions_per_stream = 2;
    static constexpr auto k_max_num_sessions = k_max_num_redundant_streams * k_max_num_rtp_sessions_per_stream;

    using ArrayOfSessions = std::array<Session, k_max_num_rtp_sessions_per_stream>;
    using ArrayOfFilters = std::array<Filter, k_max_num_rtp_sessions_per_stream>;
    using ArrayOfAddresses = std::array<boost::asio::ip::address_v4, k_max_num_rtp_sessions_per_stream>;
    using PacketBuffer = std::array<uint8_t, aes67::constants::k_max_mtu>;
    using PacketFifo =
        boost::lockfree::spsc_queue<PacketBuffer, boost::lockfree::capacity<20>, boost::lockfree::fixed_sized<true>>;

    enum class State {
        /// Available to be setup
        available = 0,
        /// Ready to be processed by the network thread
        ready,
        /// The network thread should stop using this entity, and change state to `ready_to_be_closed`
        should_be_closed,
        /// Released by the network thread, ready to be closed and put back into available state
        ready_to_be_closed,
    };

    struct SocketWithContext {
        explicit SocketWithContext(boost::asio::io_context& io_context) : socket(io_context) {}

        boost::asio::ip::udp::socket socket;
        boost::asio::ip::udp::endpoint connection_endpoint;
        boost::asio::ip::address_v4 interface_address;
        std::atomic<State> state {State::available};
    };

    struct RedundantStream {
        Id associated_id;
        ArrayOfSessions sessions;
        ArrayOfFilters filters;
        PacketFifo fifo;
        Ringbuffer receive_buffer;
        std::atomic<State> state {State::available};
    };

    boost::container::static_vector<RedundantStream, k_max_num_redundant_streams> streams;
    boost::container::static_vector<SocketWithContext, k_max_num_sessions> sockets;

    bool add_stream(
        Id id, const ArrayOfSessions& sessions, const ArrayOfFilters& filters,
        const ArrayOfAddresses& interface_addresses, boost::asio::io_context& io_context
    );
    void do_high_prio_processing();
};

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
    bool
    subscribe(Subscriber* subscriber, const Session& session, const boost::asio::ip::address_v4& interface_address);

    /**
     * Removes a subscriber from all sessions of the receiver.
     * @param subscriber The subscriber to remove.
     * @return true if the subscriber was removed, or false if it wasn't found.
     */
    bool unsubscribe(const Subscriber* subscriber);

  private:
    class SessionContext: UdpReceiver::Subscriber {
      public:
        explicit SessionContext(
            UdpReceiver& udp_receiver, Session session, boost::asio::ip::address_v4 interface_address
        );
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
    SessionContext*
    create_new_session_context(const Session& session, const boost::asio::ip::address_v4& interface_address);
    SessionContext*
    find_or_create_session_context(const Session& session, const boost::asio::ip::address_v4& interface_address);
};

}  // namespace rav::rtp
