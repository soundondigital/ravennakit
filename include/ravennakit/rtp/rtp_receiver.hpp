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

#include "rtcp_packet_view.hpp"
#include "rtp_packet_view.hpp"
#include "ravennakit/core/event_emitter.hpp"

#include <asio.hpp>

namespace rav {

struct rtp_packet_event {
    rtp_packet_view packet;
};

struct rtcp_packet_event {
    rtcp_packet_view packet;
};

class rtp_receiver final: public event_emitter<rtp_receiver, rtp_packet_event, rtcp_packet_event> {
  public:
    rtp_receiver() = delete;
    ~rtp_receiver() override;

    /**
     * Constructs a new RTP receiver using given loop.
     * @param io_context The io_context to use.
     */
    explicit rtp_receiver(asio::io_context& io_context);

    rtp_receiver(const rtp_receiver&) = delete;
    rtp_receiver& operator=(const rtp_receiver&) = delete;

    rtp_receiver(rtp_receiver&&) = delete;
    rtp_receiver& operator=(rtp_receiver&&) = delete;

    /**
     * Binds 2 UDP sockets to the given address and port. One for receiving RTP packets and one for receiving RTCP
     * packets. The sockets will be bound to the given address and port. The port number will be used for RTP and port
     * number + 1 will be used for RTCP.
     * @param address The address to bind to.
     * @param port The port to bind to. Default is 5004.
     * @return A result indicating success or failure.
     */
    void bind(const std::string& address, uint16_t port = 5004);

    /**
     * Sets the multicast membership for the given multicast address and interface address.
     * @param multicast_address The multicast address to join or leave.
     * @param interface_address The interface address to use.
     * @return A result indicating success or failure.
     */
    void join_multicast_group(
        const std::string& multicast_address, const std::string& interface_address
    );

    /**
     * @return Starts receiving datagrams on the bound sockets.
     */
    void start();

    /**
     * Stops receiving datagrams on the bound sockets.
     */
    void stop();

  private:
    asio::ip::udp::socket rtp_socket_;
    asio::ip::udp::socket rtcp_socket_;
    asio::ip::udp::endpoint rtp_endpoint_;
    asio::ip::udp::endpoint rtcp_endpoint_;
    std::array<uint8_t, 1500> rtp_data_ {};
    std::array<uint8_t, 1500> rtcp_data_ {};
    bool is_running_ = false;

    void receive_rtp();
    void receive_rtcp();
};

}  // namespace rav

