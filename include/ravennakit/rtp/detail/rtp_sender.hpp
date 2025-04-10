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
#include "ravennakit/core/containers/buffer_view.hpp"

namespace rav::rtp {

/**
 * This class is responsible for sending RTP packets.
 * - Maintains a socket to send RTP packets.
 * - Maintains a socket to send RTCP packets (maybe the same socket).
 */
class Sender {
  public:
    Sender(asio::io_context& io_context, const asio::ip::address_v4& interface_address) :
        socket_(io_context), interface_address_(interface_address) {
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
    void send_to(const ByteBuffer& packet, const asio::ip::udp::endpoint& endpoint) {
        RAV_ASSERT(packet.data() != nullptr, "Packet data is null");
        RAV_ASSERT(packet.size() > 0, "Packet size is 0");
        socket_.send_to(asio::buffer(packet.data(), packet.size()), endpoint);
    }

    /**
     * Sends given data as an RTP packet.
     * @param packet Encoded RTP packet.
     * @param endpoint The endpoint to send the packet to.
     */
    void send_to(const BufferView<const uint8_t>& packet, const asio::ip::udp::endpoint& endpoint) {
        RAV_ASSERT(packet.data() != nullptr, "Packet data is null");
        RAV_ASSERT(!packet.empty(), "Packet is empty");
        socket_.send_to(asio::buffer(packet.data(), packet.size()), endpoint);
    }

    /**
     * Sends given data as an RTP packet.
     * @param data Pointer to the data to send.
     * @param data_size The size of the data to send.
     * @param endpoint The endpoint to send the packet to.
     */
    void send_to(const uint8_t* data, const size_t data_size, const asio::ip::udp::endpoint& endpoint) {
        RAV_ASSERT(data != nullptr, "Packet data is null");
        RAV_ASSERT(data_size != 0, "Packet is empty");
        socket_.send_to(asio::buffer(data, data_size), endpoint);
    }

    /**
     * Sets the interface to use for this sender.
     * @param interface_address The address of the interface to use.
     */
    void set_interface(const asio::ip::address_v4& interface_address) {
        asio::error_code ec;
        socket_.set_option(asio::ip::multicast::outbound_interface(interface_address), ec);
        if (ec) {
            RAV_ERROR("Failed to set interface: {}", ec.message());
        }
        interface_address_ = interface_address;
    }

    /**
     * @return The interface address used by the sender.
     */
    [[nodiscard]] const asio::ip::address_v4& get_interface_address() const {
        // TODO: Get the interface address from the socket (local endpoint)
        return interface_address_;
    }

  private:
    asio::ip::udp::socket socket_;
    asio::ip::address_v4 interface_address_;
};

}  // namespace rav::rtp
