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
#include "udp_sender_receiver.hpp"
#include "ravennakit/rtp/rtp_packet.hpp"

namespace rav::rtp {

/**
 * This class is responsible for transmitting RTP packets.
 * - Maintains a socket to send RTP packets.
 * - Maintains a socket to send RTCP packets (maybe the same socket).
 */
class Transmitter {
  public:
    Transmitter(asio::io_context& io_context, const asio::ip::address_v4& interface_address) : socket_(io_context) {
        socket_.open(asio::ip::udp::v4());
        socket_.set_option(asio::ip::multicast::outbound_interface(interface_address));
        socket_.set_option(asio::ip::multicast::enable_loopback(false));
        socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    }

    /**
     * Sends given data as an RTP packet.
     * @param packet Encoded RTP packet.
     * @param endpoint The endpoint to send the packet to.
     */
    void send_to(const byte_buffer& packet, const asio::ip::udp::endpoint& endpoint) {
        RAV_ASSERT(packet.data() != nullptr, "Packet data is null");
        RAV_ASSERT(packet.size() > 0, "Packet size is 0");
        socket_.send_to(asio::buffer(packet.data(), packet.size()), endpoint);
    }

  private:
    byte_buffer buffer_;
    asio::ip::udp::socket socket_;
};

}  // namespace rav
