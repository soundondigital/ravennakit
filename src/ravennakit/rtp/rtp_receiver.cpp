/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/log.hpp"
#include "ravennakit/rtp/rtcp_packet_view.hpp"
#include "ravennakit/rtp/rtp_receiver.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/util/tracy.hpp"

#include <fmt/core.h>

rav::rtp_receiver::rtp_receiver(asio::io_context& io_context) : rtp_socket_(io_context), rtcp_socket_(io_context) {}

rav::rtp_receiver::~rtp_receiver() {
    try {
        stop();
    } catch (const std::exception& e) {
        RAV_ERROR("Failed to stop RTP receiver: {}", e.what());
    } catch (...) {
        RAV_ERROR("Failed to stop RTP receiver: unknown error");
    }
}

void rav::rtp_receiver::bind(const std::string& address, const uint16_t port) {
    asio::ip::udp::endpoint listen_endpoint(asio::ip::make_address(address), port);
    rtp_socket_.open(listen_endpoint.protocol());
    rtp_socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    rtp_socket_.bind(listen_endpoint);

    listen_endpoint.port(port + 1);
    rtcp_socket_.open(listen_endpoint.protocol());
    rtcp_socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    rtcp_socket_.bind(listen_endpoint);
}

void rav::rtp_receiver::join_multicast_group(
    const std::string& multicast_address, const std::string& interface_address
) {
    rtp_socket_.set_option(asio::ip::multicast::join_group(
        asio::ip::make_address(multicast_address).to_v4(), asio::ip::make_address(interface_address).to_v4()
    ));

    rtcp_socket_.set_option(asio::ip::multicast::join_group(
        asio::ip::make_address(multicast_address).to_v4(), asio::ip::make_address(interface_address).to_v4()
    ));
}

void rav::rtp_receiver::start() {
    TRACY_ZONE_SCOPED;

    if (is_running_) {
        RAV_WARNING("RTP receiver is already running");
        return;
    }
    is_running_ = true;
    receive_rtp();
    receive_rtcp();
}

void rav::rtp_receiver::stop() {
    // No need to call shutdown on the sockets as they are datagram sockets.

    rtp_socket_.close();
    rtcp_socket_.close();
    is_running_ = false;
}

void rav::rtp_receiver::receive_rtp() {
    rtp_socket_.async_receive_from(
        asio::buffer(rtp_data_), rtp_endpoint_,
        [this](const std::error_code& ec, const std::size_t length) {
            TRACY_ZONE_SCOPED;
            if (!ec) {
                const rtp_packet_view rtp_packet(rtp_data_.data(), length);
                emit(rtp_packet_event {rtp_packet});
                receive_rtp();
            } else {
                RAV_ERROR("RTP receiver error: {}", ec.message());
            }
        }
    );
}

void rav::rtp_receiver::receive_rtcp() {
    rtcp_socket_.async_receive_from(
        asio::buffer(rtcp_data_), rtcp_endpoint_,
        [this](const std::error_code& ec, const std::size_t length) {
            if (!ec) {
                const rtcp_packet_view rtcp_packet(rtcp_data_.data(), length);
                emit(rtcp_packet_event {rtcp_packet});
                receive_rtcp();
            } else {
                RAV_ERROR("RTCP receiver error: {}", ec.message());
            }
        }
    );
}
