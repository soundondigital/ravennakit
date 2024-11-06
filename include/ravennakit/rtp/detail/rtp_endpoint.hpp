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

#include "ravennakit/rtp/rtcp_packet_view.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"

#include <asio.hpp>

namespace rav {

/**
 * A class which sets up sockets for receiver RTP and RTCP packets, and allows subscribing to the received packets.
 * Given endpoint will be used for binding to the local address. The port number will be used for the RTP socket, and
 * port number + 1 will be used for the RTCP socket.
 */
class rtp_endpoint: public std::enable_shared_from_this<rtp_endpoint> {
  public:
    struct rtp_packet_event {
        const rtp_packet_view& packet;
    };

    struct rtcp_packet_event {
        const rtcp_packet_view& packet;
    };

    class handler {
      public:
        virtual ~handler() = default;
        virtual void on(const rtp_packet_event& rtp_event) = 0;
        virtual void on(const rtcp_packet_event& rtcp_event) = 0;
    };

    static std::shared_ptr<rtp_endpoint> create(asio::io_context& io_context, asio::ip::udp::endpoint endpoint);

    ~rtp_endpoint();

    rtp_endpoint(const rtp_endpoint&) = delete;
    rtp_endpoint& operator=(const rtp_endpoint&) = delete;

    rtp_endpoint(rtp_endpoint&&) = delete;
    rtp_endpoint& operator=(rtp_endpoint&&) = delete;

    [[nodiscard]] asio::ip::udp::endpoint local_rtp_endpoint() const;
    void start(handler& handler);
    void stop();

  private:
    handler* handler_ = nullptr;
    asio::ip::udp::socket rtp_socket_;
    asio::ip::udp::socket rtcp_socket_;
    asio::ip::udp::endpoint rtp_sender_endpoint_;   // For receiving the senders address.
    asio::ip::udp::endpoint rtcp_sender_endpoint_;  // For receiving the senders address.
    std::array<uint8_t, 1500> rtp_data_ {};
    std::array<uint8_t, 1500> rtcp_data_ {};
    bool is_running_ = false;

    explicit rtp_endpoint(asio::io_context& io_context, asio::ip::udp::endpoint endpoint);
    void receive_rtp();
    void receive_rtcp();
};

}  // namespace rav
