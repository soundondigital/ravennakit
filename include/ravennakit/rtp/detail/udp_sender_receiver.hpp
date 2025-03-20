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

#include "ravennakit/core/subscription.hpp"

#include <asio.hpp>

namespace rav::rtp {

/**
 * A custom UDP sender and receiver class which extends usual UDP socket functionality by adding the ability to receive
 * the destination address of a received packet. This is useful for RTP where sessions are defined by the source and
 * destination endpoints. Also in cases where a single receiver is receiving from multiple senders, the destination
 * address is needed to determine the source of the packet.
 */
class UdpSenderReceiver {
  public:
    struct recv_event {
        const uint8_t* data;
        size_t size;
        const asio::ip::udp::endpoint& src_endpoint;
        const asio::ip::udp::endpoint& dst_endpoint;
        uint64_t recv_time; // Monotonically increasing time in nanoseconds with arbitrary starting point.
    };

    using HandlerType = std::function<void(const recv_event& event)>;

    /**
     * Construct a new instance of the class. Private to force the use of the factory methods.
     * @param io_context The asio io_context to use.
     * @param endpoint The endpoint to bind to.
     */
    UdpSenderReceiver(asio::io_context& io_context, const asio::ip::udp::endpoint& endpoint);

    /**
     * Construct a new instance of the class. Private to force the use of the factory methods.
     * @param io_context The asio io_context to use.
     * @param interface_address The address to bind to.
     * @param port The port to bind to.
     */
    UdpSenderReceiver(asio::io_context& io_context, const asio::ip::address& interface_address, uint16_t port);

    UdpSenderReceiver(const UdpSenderReceiver&) = delete;
    UdpSenderReceiver& operator=(const UdpSenderReceiver&) = delete;

    UdpSenderReceiver(UdpSenderReceiver&&) noexcept = delete;
    UdpSenderReceiver& operator=(UdpSenderReceiver&&) noexcept = delete;

    ~UdpSenderReceiver();

    /**
     * Start the receiver.
     * @param handler The handler to receive incoming packets.
     */
    void start(HandlerType handler) const;

    /**
     * Sends a datagram to the given endpoint.
     * @param data The data to send.
     * @param size The size of the data. Must be smaller than MTU.
     * @param endpoint The endpoint to send the data to.
     */
    void send(const uint8_t* data, size_t size, const asio::ip::udp::endpoint& endpoint) const;

    /**
     * Join a multicast group. A group can be joined multiple times as the group will be counted internally. Only when
     * the last subscription is removed will the group be left.
     * @param multicast_address The multicast address to join.
     * @param interface_address The interface address to join the multicast group on.
     * @returns A subscription object which will leave the multicast group when it goes out of scope.
     */
    [[nodiscard]] subscription
    join_multicast_group(const asio::ip::address& multicast_address, const asio::ip::address& interface_address) const;

    /**
     * Set the outbound interface for multicast packets.
     * @param interface_address The address of the interface to use.
     * @return True if the operation was successful, false otherwise.
     */
    [[nodiscard]] asio::error_code
    set_multicast_outbound_interface(const asio::ip::address_v4& interface_address) const;

    /**
     * Set the multicast loopback option.
     * @param enable True to enable multicast loopback, false to disable.
     * @return True if the operation was successful, false otherwise.
     */
    [[nodiscard]] asio::error_code set_multicast_loopback(bool enable) const;

    /**
     * Set the DSCP value for the socket. The value will be shifted 2 bits to the left, which sets the ECN bits to zero.
     * @param value The DSCP value to set.
     */
    void set_dscp_value(int value) const;

  private:
    class impl;
    std::shared_ptr<impl> impl_;
};

}  // namespace rav
