/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/detail/rtp_endpoint.hpp"

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/tracy.hpp"

rav::rtp_endpoint::rtp_endpoint(asio::io_context& io_context, asio::ip::udp::endpoint endpoint) :
    rtp_socket_(io_context), rtcp_socket_(io_context) {
    // RTP
    rtp_socket_.open(endpoint.protocol());
    rtp_socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    rtp_socket_.bind(endpoint);

    // RTCP
    endpoint.port(endpoint.port() + 1);
    rtcp_socket_.open(endpoint.protocol());
    rtcp_socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    rtcp_socket_.bind(endpoint);
}

std::shared_ptr<rav::rtp_endpoint>
rav::rtp_endpoint::create(asio::io_context& io_context, asio::ip::udp::endpoint endpoint) {
    return std::shared_ptr<rtp_endpoint>(new rtp_endpoint(io_context, std::move(endpoint)));
}

rav::rtp_endpoint::~rtp_endpoint() {
    try {
        stop();
    } catch (const std::exception& e) {
        RAV_ERROR("Failed to stop RTP receiver: {}", e.what());
    } catch (...) {
        RAV_ERROR("Failed to stop RTP receiver: unknown error");
    }
}

asio::ip::udp::endpoint rav::rtp_endpoint::local_rtp_endpoint() const {
    return rtp_socket_.local_endpoint();
}

void rav::rtp_endpoint::start(handler& handler) {
    TRACY_ZONE_SCOPED;

    if (&handler == handler_) {
        RAV_WARNING("RTP receiver is already running with the same handler");
        return;
    }

    if (handler_ != nullptr) {
        RAV_WARNING("RTP receiver is already running");
        return;
    }

    handler_ = &handler;
    receive_rtp();
    receive_rtcp();
}

void rav::rtp_endpoint::stop() {
    handler_ = nullptr;
    // No need to call shutdown on the sockets as they are datagram sockets.
    rtp_socket_.close();
    rtcp_socket_.close();
}

void rav::rtp_endpoint::receive_rtp() {
    auto self = shared_from_this();
    rtp_socket_.async_receive_from(
        asio::buffer(rtp_data_), rtp_sender_endpoint_,
        [self](const std::error_code& ec, const std::size_t length) {
            TRACY_ZONE_SCOPED;
            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    RAV_TRACE("Operation aborted");
                    return;
                }
                if (ec == asio::error::eof) {
                    RAV_TRACE("EOF");
                    return;
                }
                RAV_ERROR("Read error: {}. Closing connection.", ec.message());
                return;
            }
            if (self->handler_ == nullptr) {
                RAV_ERROR("Handler is null. Closing connection.");
                return;
            }
            const rtp_packet_view rtp_packet(self->rtp_data_.data(), length);
            if (rtp_packet.validate()) {
                const rtp_packet_event event {rtp_packet};
                self->handler_->on(event);
            } else {
                RAV_WARNING("Invalid RTP packet received. Ignoring.");
            }
            self->receive_rtp();  // Schedule another round of receiving.
        }
    );
}

void rav::rtp_endpoint::receive_rtcp() {
    auto self = shared_from_this();
    rtcp_socket_.async_receive_from(
        asio::buffer(rtcp_data_), rtcp_sender_endpoint_,
        [self](const std::error_code& ec, const std::size_t length) {
            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    RAV_TRACE("Operation aborted");
                    return;
                }
                if (ec == asio::error::eof) {
                    RAV_TRACE("EOF");
                    return;
                }
                RAV_ERROR("Read error: {}. Closing connection.", ec.message());
                return;
            }
            if (self->handler_ == nullptr) {
                RAV_ERROR("Owner is null. Closing connection.");
                return;
            }
            const rtcp_packet_view rtcp_packet(self->rtcp_data_.data(), length);
            if (rtcp_packet.validate()) {
                const rtcp_packet_event event {rtcp_packet};
                self->handler_->on(event);
            } else {
                RAV_WARNING("Invalid RTCP packet received. Ignoring.");
            }
            self->receive_rtcp();  // Schedule another round of receiving.
        }
    );
}
