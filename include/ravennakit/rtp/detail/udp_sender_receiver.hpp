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

#if RAV_APPLE
    #define IP_RECVDSTADDR_PKTINFO IP_RECVDSTADDR
#else
    #define IP_RECVDSTADDR_PKTINFO IP_PKTINFO
#endif

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/tracy.hpp"
#include "ravennakit/core/subscription.hpp"

#include <asio.hpp>

#include <memory>

namespace rav {

/**
 * A custom UDP sender and receiver class which extends usual UDP socket functionality by adding the ability to receive
 * the destination address of a received packet. This is useful for RTP where sessions are defined by the source and
 * destination endpoints. Also in cases where a single receiver is receiving from multiple senders, the destination
 * address is needed to determine the source of the packet.
 * The class itself can only be used as shared instance because it will keep itself alive when there are pending
 * callbacks to manage the lifetime.
 */
class udp_sender_receiver: public std::enable_shared_from_this<udp_sender_receiver> {
  public:
    using handler_type = std::function<
        void(uint8_t* data, size_t size, const asio::ip::udp::endpoint& src, const asio::ip::udp::endpoint& dst)>;

    /**
     * Create a new instance of the class.
     * @param io_context The asio io_context to use.
     * @param endpoint The endpoint to bind to.
     * @throws std::exception when creation fails.
     * @return A shared pointer to the new instance.
     */
    static std::shared_ptr<udp_sender_receiver>
    make(asio::io_context& io_context, const asio::ip::udp::endpoint& endpoint) {
        return std::shared_ptr<udp_sender_receiver>(new udp_sender_receiver(io_context, endpoint));
    }

    /**
     * Create a new instance of the class.
     * @param io_context The asio io_context to use.
     * @param interface_address The address to bind to.
     * @param port The port to bind to.
     * @throws std::exception when creation fails.
     * @return A shared pointer to the new instance.
     */
    static std::shared_ptr<udp_sender_receiver>
    make(asio::io_context& io_context, const asio::ip::address& interface_address, const uint16_t port) {
        return std::shared_ptr<udp_sender_receiver>(new udp_sender_receiver(io_context, interface_address, port));
    }

    /**
     * Start the receiver.
     * @param handler The handler to receive incoming packets.
     */
    void start(handler_type handler);

    /**
     * Stop the receiver. If the receiver is not running then this method doesn't have any effect.
     */
    void stop();

    /**
     * Join a multicast group.
     * @param multicast_address The multicast address to join.
     * @param interface_address The interface address to join the multicast group on.
     */
    void join_multicast_group(const asio::ip::address& multicast_address, const asio::ip::address& interface_address);

  private:
    asio::ip::udp::socket socket_;
    asio::ip::udp::endpoint sender_endpoint_ {};  // For receiving the senders address.
    std::array<uint8_t, 1500> recv_data_ {};
    handler_type handler_;

    /**
     * Construct a new instance of the class. Private to force the use of the factory methods.
     * @param io_context The asio io_context to use.
     * @param endpoint The endpoint to bind to.
     */
    udp_sender_receiver(asio::io_context& io_context, const asio::ip::udp::endpoint& endpoint);

    /**
     * Construct a new instance of the class. Private to force the use of the factory methods.
     * @param io_context The asio io_context to use.
     * @param interface_address The address to bind to.
     * @param port The port to bind to.
     */
    udp_sender_receiver(asio::io_context& io_context, const asio::ip::address& interface_address, uint16_t port);

    /**
     * Start the async receive loop.
     */
    void async_receive();

    /**
     * Receive a packet from the socket. Implemented differently for Windows and the other platforms.
     * @param socket The socket to receive from.
     * @param data_buf The buffer to receive the data into.
     * @param self The shared pointer to the instance.
     * @param src_endpoint The source endpoint.
     * @param dst_endpoint The destination endpoint.
     * @param ec The error code.
     * @return The number of bytes received, or zero if an error occurred.
     */
    static size_t receive_from_socket(
        asio::ip::udp::socket& socket, std::array<uint8_t, 1500>& data_buf,
        const std::shared_ptr<udp_sender_receiver>& self, asio::ip::udp::endpoint& src_endpoint,
        asio::ip::udp::endpoint& dst_endpoint, asio::error_code& ec
    );
};

}  // namespace rav
