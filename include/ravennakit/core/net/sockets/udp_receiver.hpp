/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "extended_udp_socket.hpp"
#include "ravennakit/core/util/subscriber_list.hpp"

namespace rav {

/**
 * This class exists because it is complicated (impossible) to receive both unicast and multicast traffic across
 * different sockets from the same network interface. Especially Windows is bad in this where it isn't possible to bind
 * a socket to a multicast address. This means that if both unicast and multicast connections on the same port are to be
 * supported, only a single socket can be used. This is cumbersome and impractical but luckily this class makes it easy
 * to subscribe to incoming traffic without thinking about sockets. Due to the scope of the socket capabilities which
 * are tied to the whole process it makes sense to share an instance of this class process wide.
 * Read more: https://stackoverflow.com/questions/6140734/cannot-bind-to-multicast-address-windows
 */
class UdpReceiver {
  public:
    class Subscriber {
      public:
        virtual ~Subscriber() = default;
        virtual void on_receive(const ExtendedUdpSocket::RecvEvent& event) = 0;
    };

    explicit UdpReceiver(boost::asio::io_context& io_context);

    /**
     * Subscribes to traffic destined for `port` on interface identified by `interface_address`. The address can be the
     * ipv4 'any' address, but beware that after another subscriber subscribes to a specific interface the 'any' socket
     * will not receive unicast traffic anymore.
     * To subscribe to multicast traffic use the other overload of this function.
     * @param subscriber The subscriber interested in the traffic.
     * @param interface_address The address to bind to. This can be the ipv4 any address or an interface address.
     * @param port The port to bind to.
     * @returns true if the subscription was successful, false otherwise.
     */
    [[nodiscard]] bool subscribe(Subscriber* subscriber, const boost::asio::ip::address_v4& interface_address, uint16_t port);

    /**
     * Subscribes to multicast traffic on the given multicast address and interface address. The interface address
     * should not be the ipv4 'any' address. The multicast address should be a valid multicast address.
     * Note that each subscriber can only subscribe to a single multicast address per specific port.
     * @param subscriber The subscriber interested in the traffic.
     * @param multicast_address The multicast address to subscribe to.
     * @param interface_address The address to bind to. This should not be the ipv4 any address.
     * @param port The port to bind to.
     * @returns true if the subscription was successful, false otherwise.
     */
    [[nodiscard]] bool subscribe(
        Subscriber* subscriber, const boost::asio::ip::address_v4& multicast_address,
        const boost::asio::ip::address_v4& interface_address, uint16_t port
    );

    /**
     * Unsubscribes from all traffic for the given subscriber. This will remove the subscriber from all sockets and all
     * multicast groups.
     * @param subscriber The subscriber to unsubscribe.
     */
    void unsubscribe(const Subscriber* subscriber);

  private:
    struct MulticastGroup {
        boost::asio::ip::address_v4 multicast_address;
        boost::asio::ip::address_v4 interface_address;

        bool operator==(const MulticastGroup& other) const {
            return multicast_address == other.multicast_address && interface_address == other.interface_address;
        }
    };

    class SocketContext {
      public:
        SocketContext(boost::asio::io_context& io_context, boost::asio::ip::udp::endpoint local_endpoint);

        [[nodiscard]] bool add_subscriber(Subscriber* subscriber);
        [[nodiscard]] bool add_subscriber(Subscriber* subscriber, const MulticastGroup& multicast_group);
        [[nodiscard]] bool remove_subscriber(const Subscriber* subscriber);
        [[nodiscard]] const boost::asio::ip::udp::endpoint& local_endpoint() const;
        [[nodiscard]] bool empty() const;

      private:
        boost::asio::ip::udp::endpoint local_endpoint_;
        SubscriberList<Subscriber, MulticastGroup> subscribers_;
        ExtendedUdpSocket socket_;

        [[nodiscard]] bool has_multicast_group_subscriber(const MulticastGroup& multicast_group) const;
        void handle_recv_event(const ExtendedUdpSocket::RecvEvent& event);
    };

    boost::asio::io_context& io_context_;
    std::vector<std::unique_ptr<SocketContext>> sockets_;

    [[nodiscard]] SocketContext* find_socket_context(const boost::asio::ip::udp::endpoint& endpoint) const;
    SocketContext& find_or_create_socket_context(const boost::asio::ip::udp::endpoint& endpoint);
    size_t cleanup_empty_sockets();
};

}  // namespace rav
