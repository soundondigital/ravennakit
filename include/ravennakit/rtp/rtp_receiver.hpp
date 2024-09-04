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

#include <uvw/loop.h>
#include <uvw/udp.h>

#include "ravennakit/core/result.hpp"
#include "rtcp_packet_view.hpp"
#include "rtp_packet_view.hpp"

namespace rav {

struct rtp_packet_event {
    rtp_packet_view packet;
};

struct rtcp_packet_event {
    rtcp_packet_view packet;
};

class rtp_receiver final: public uvw::emitter<rtp_receiver, rtp_packet_event, rtcp_packet_event> {
  public:
    using udp_flags = uvw::udp_handle::udp_flags;

    rtp_receiver() = delete;
    ~rtp_receiver() override;

    /**
     * Constructs a new RTP receiver using given loop.
     * @param loop The event loop to use.
     */
    explicit rtp_receiver(const std::shared_ptr<uvw::loop>& loop);

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
     * @param opts The options to pass to the underlying UDP sockets. By default, REUSEADDR is used.
     * @return A result indicating success or failure.
     */
    void bind(const std::string& address, uint16_t port = 5004, udp_flags opts = udp_flags::REUSEADDR) const;

    /**
     * Sets the multicast membership for the given multicast address and interface address.
     * @param multicast_address The multicast address to join or leave.
     * @param interface_address The interface address to use.
     * @param membership The membership to set.
     * @return A result indicating success or failure.
     */
    void set_multicast_membership(
        const std::string& multicast_address, const std::string& interface_address,
        uvw::udp_handle::membership membership
    ) const;

    /**
     * @return Starts receiving datagrams on the bound sockets.
     */
    void start() const;

    /**
     * Stops receiving datagrams on the bound sockets.
     */
    void stop() const;

    /**
     * Closes the sockets. Implies stop().
     * @returns A result indicating success or failure.
     */
    void close() const;

  private:
    std::shared_ptr<uvw::loop> loop_;
    std::shared_ptr<uvw::udp_handle> rtp_socket_;
    std::shared_ptr<uvw::udp_handle> rtcp_socket_;
};

}  // namespace rav
