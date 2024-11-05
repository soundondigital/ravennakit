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

#include "ravennakit/core/subscriber_list.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/core/tracy.hpp"

#include <fmt/core.h>

class rav::rtp_receiver::impl: public std::enable_shared_from_this<impl> {
  public:
    explicit impl(asio::io_context& io_context, rtp_receiver& owner) :
        owner_(&owner), rtp_socket_(io_context), rtcp_socket_(io_context) {}

    impl(const impl&) = delete;
    impl& operator=(const impl&) = delete;

    impl(impl&&) = delete;
    impl& operator=(impl&&) = delete;

    ~impl() {
        try {
            stop();
        } catch (const std::exception& e) {
            RAV_ERROR("Failed to stop RTP receiver: {}", e.what());
        } catch (...) {
            RAV_ERROR("Failed to stop RTP receiver: unknown error");
        }
    }

    void bind(const std::string& address, const uint16_t port) {
        asio::ip::udp::endpoint listen_endpoint(asio::ip::make_address(address), port);
        rtp_socket_.open(listen_endpoint.protocol());
        rtp_socket_.set_option(asio::ip::udp::socket::reuse_address(true));
        rtp_socket_.bind(listen_endpoint);

        listen_endpoint.port(port + 1);
        rtcp_socket_.open(listen_endpoint.protocol());
        rtcp_socket_.set_option(asio::ip::udp::socket::reuse_address(true));
        rtcp_socket_.bind(listen_endpoint);
    }

    void join_multicast_group(const std::string& multicast_address, const std::string& interface_address) {
        rtp_socket_.set_option(asio::ip::multicast::join_group(
            asio::ip::make_address(multicast_address).to_v4(), asio::ip::make_address(interface_address).to_v4()
        ));

        rtcp_socket_.set_option(asio::ip::multicast::join_group(
            asio::ip::make_address(multicast_address).to_v4(), asio::ip::make_address(interface_address).to_v4()
        ));
    }

    void start() {
        TRACY_ZONE_SCOPED;

        if (is_running_) {
            RAV_WARNING("RTP receiver is already running");
            return;
        }
        is_running_ = true;
        receive_rtp();
        receive_rtcp();
    }

    void stop() {
        // No need to call shutdown on the sockets as they are datagram sockets.
        rtp_socket_.close();
        rtcp_socket_.close();
        is_running_ = false;
    }

    void reset_owner() {
        owner_ = nullptr;
    }

  private:
    rtp_receiver* owner_ {};
    asio::ip::udp::socket rtp_socket_;
    asio::ip::udp::socket rtcp_socket_;
    asio::ip::udp::endpoint rtp_sender_endpoint_;   // For receiving the senders address.
    asio::ip::udp::endpoint rtcp_sender_endpoint_;  // For receiving the senders address.
    std::array<uint8_t, 1500> rtp_data_ {};
    std::array<uint8_t, 1500> rtcp_data_ {};
    bool is_running_ = false;

    void receive_rtp() {
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
                if (self->owner_ == nullptr) {
                    RAV_ERROR("Owner is null. Closing connection.");
                    return;
                }
                const rtp_packet_view rtp_packet(self->rtp_data_.data(), length);
                const rtp_packet_event event {rtp_packet};
                for (auto& node : self->owner_->subscriber_nodes_) {
                    if (auto* subscriber = node->first) {
                        subscriber->on(event);
                    }
                }
                self->receive_rtp();  // Schedule another round of receiving.
            }
        );
    }

    void receive_rtcp() {
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
                if (self->owner_ == nullptr) {
                    RAV_ERROR("Owner is null. Closing connection.");
                    return;
                }
                const rtcp_packet_view rtcp_packet(self->rtcp_data_.data(), length);
                const rtcp_packet_event event {rtcp_packet};
                for (auto& node : self->owner_->subscriber_nodes_) {
                    if (auto* subscriber = node->first) {
                        subscriber->on(event);
                    }
                }
                self->receive_rtcp();  // Schedule another round of receiving.
            }
        );
    }
};

rav::rtp_receiver::rtp_receiver(asio::io_context& io_context) : impl_(std::make_unique<impl>(io_context, *this)) {}

rav::rtp_receiver::~rtp_receiver() {
    impl_->reset_owner();
    for (auto& node : subscriber_nodes_) {
        node.reset();
    }
}

void rav::rtp_receiver::subscriber::subscribe(rtp_receiver& receiver) {
    RAV_ASSERT(receiver.impl_ != nullptr, "Expecting valid receiver implementation");
    *node_ = {this, &receiver};
    receiver.subscriber_nodes_.push_back(node_);
}

void rav::rtp_receiver::bind(const std::string& address, const uint16_t port) const {
    impl_->bind(address, port);
}

void rav::rtp_receiver::join_multicast_group(const std::string& multicast_address, const std::string& interface_address)
    const {
    impl_->join_multicast_group(multicast_address, interface_address);
}

void rav::rtp_receiver::start() const {
    impl_->start();
}

void rav::rtp_receiver::stop() const {
    impl_->stop();
}
