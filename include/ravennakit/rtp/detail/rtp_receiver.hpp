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
#include "ravennakit/core/net/asio/asio_helpers.hpp"
#include "ravennakit/core/util/subscriber_list.hpp"
#include "ravennakit/core/net/sockets/extended_udp_socket.hpp"
#include "ravennakit/core/net/sockets/udp_receiver.hpp"
#include "ravennakit/core/sync/atomic_rw_lock.hpp"
#include "ravennakit/core/util/id.hpp"
#include "ravennakit/core/util/safe_function.hpp"

#include <boost/asio.hpp>
#include <boost/container/static_vector.hpp>
#include <boost/lockfree/policies.hpp>
#include <boost/lockfree/spsc_queue.hpp>

#include <utility>

namespace rav::rtp {

struct Receiver3 {
    static constexpr auto k_max_num_readers = 16;
    static constexpr auto k_max_num_redundant_sessions = 2;
    static constexpr auto k_max_num_sessions = k_max_num_readers * k_max_num_redundant_sessions;

    using ArrayOfSessions = std::array<Session, k_max_num_redundant_sessions>;
    using ArrayOfFilters = std::array<Filter, k_max_num_redundant_sessions>;
    using ArrayOfAddresses = std::array<ip_address_v4, k_max_num_redundant_sessions>;
    using PacketBuffer = std::array<uint8_t, aes67::constants::k_mtu>;
    using PacketFifo =
        boost::lockfree::spsc_queue<PacketBuffer, boost::lockfree::capacity<20>, boost::lockfree::fixed_sized<true>>;

    enum class State {
        /// Available to be setup
        available = 0,
        /// Ready to be processed by the network thread
        ready,
        /// The network thread should stop using this entity, and change state to `available`
        should_be_closed,
    };

    struct SocketWithContext {
        explicit SocketWithContext(boost::asio::io_context& io_context) : socket(io_context) {}

        udp_socket socket;
        uint16_t port {};
        AtomicRwLock rw_lock;
    };

    /**
     * Holds the structures to receive incoming data from redundant sources into a single buffer.
     */
    struct Reader {
        Id id;  // To associate with another entity, and to determine whether this reader is already being used.
        ArrayOfSessions sessions;
        ArrayOfFilters filters;
        ArrayOfAddresses interfaces;
        PacketFifo fifo;
        Ringbuffer receive_buffer;
        AtomicRwLock rw_lock;
    };

    boost::container::static_vector<Reader, k_max_num_readers> readers;
    boost::container::static_vector<SocketWithContext, k_max_num_sessions> sockets;

    /// Function for joining a multicast group. Can be overridden to alter behaviour. Used for unit testing.
    SafeFunction<bool(udp_socket&, ip_address_v4, ip_address_v4)> join_multicast_group;

    /// Function for leaving a multicast group. Can be overridden to alter behaviour. Used for unit testing.
    SafeFunction<bool(udp_socket&, ip_address_v4, ip_address_v4)> leave_multicast_group;

    Receiver3();
    ~Receiver3();

    /**
     * Adds a reader to the receiver.
     * Thread safe: no.
     * @param id The id to use, must be unique.
     * @param sessions The sessions to receive.
     * @param filters The source filters to use.
     * @param interfaces The interfaces to receive multicast sessions on.
     * @param io_context The io_context to associate with the sockets. Can be a dummy.
     * @return true if a new reader was added, or false if not.
     */
    [[nodiscard]] bool add_reader(
        Id id, const ArrayOfSessions& sessions, const ArrayOfFilters& filters, const ArrayOfAddresses& interfaces,
        boost::asio::io_context& io_context
    );

    /**
     * Removes the reader with given id, if it exists.
     * Thread safe: no.
     * @param id The id of the reader to remove.
     * @return true if a reader was removed, or false if not.
     */
    [[nodiscard]] bool remove_reader(Id id);

    /**
     * Sets the interfaces on all readers, leaving and joining multicast groups where necessary.
     * @param interfaces The new interfaces to use.
     */
    void set_interfaces(const ArrayOfAddresses& interfaces);

    /**
     * Call this to read incoming packets and place the data inside a fifo for consumption. Should be called from a
     * single high priority thread with regular short intervals.
     */
    void read_incoming_packets();
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
