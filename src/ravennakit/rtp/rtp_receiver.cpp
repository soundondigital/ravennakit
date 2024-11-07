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

#include <utility>

#if RAV_APPLE
    #define IP_RECVDSTADDR_PKTINFO IP_RECVDSTADDR
#elif RAV_LINUX
    #define IP_RECVDSTADDR_PKTINFO IP_PKTINFO
#endif

class rav::rtp_receiver::impl: public std::enable_shared_from_this<impl> {
  public:
    explicit impl(
        asio::io_context& io_context, const asio::ip::address& interface_address, const uint16_t rtp_port,
        const uint16_t rtcp_port
    ) :
        rtp_socket_(io_context), rtcp_socket_(io_context) {
        // RTP
        asio::ip::udp::endpoint endpoint(interface_address, rtp_port);
        asio::error_code ec;
        rtp_socket_.open(endpoint.protocol(), ec);
        if (ec) {
            RAV_ERROR("Failed to open RTP socket: {}", ec.message());
        }
        rtp_socket_.set_option(asio::ip::udp::socket::reuse_address(true), ec);
        if (ec) {
            RAV_ERROR("Failed to set reuse address option on RTP socket: {}", ec.message());
        }
        rtp_socket_.bind(endpoint, ec);
        if (ec) {
            RAV_ERROR("Failed to bind RTP socket: {}", ec.message());
        }
        rtp_socket_.non_blocking(true, ec);
        if (ec) {
            RAV_ERROR("Failed to set non-blocking mode on RTP socket: {}", ec.message());
        }
        rtp_socket_.set_option(asio::detail::socket_option::integer<IPPROTO_IP, IP_RECVDSTADDR_PKTINFO>(1), ec);
        if (ec) {
            RAV_ERROR("Failed to set IP_RECVDSTADDR option on RTP socket: {}", ec.message());
        }

        // RTCP
        endpoint.port(rtcp_port);
        rtcp_socket_.open(endpoint.protocol(), ec);
        if (ec) {
            RAV_ERROR("Failed to open RTCP socket: {}", ec.message());
        }
        rtcp_socket_.set_option(asio::ip::udp::socket::reuse_address(true), ec);
        if (ec) {
            RAV_ERROR("Failed to set reuse address option on RTCP socket: {}", ec.message());
        }
        rtcp_socket_.bind(endpoint, ec);
        if (ec) {
            RAV_ERROR("Failed to bind RTCP socket: {}", ec.message());
        }
        rtcp_socket_.non_blocking(true, ec);
        if (ec) {
            RAV_ERROR("Failed to set non-blocking mode on RTP socket: {}", ec.message());
        }
        rtcp_socket_.set_option(asio::detail::socket_option::integer<IPPROTO_IP, IP_RECVDSTADDR_PKTINFO>(1), ec);
        if (ec) {
            RAV_ERROR("Failed to set IP_RECVDSTADDR option on RTCP socket: {}", ec.message());
        }
    }

    void start(rtp_receiver& owner) {
        TRACY_ZONE_SCOPED;

        if (&owner == owner_) {
            RAV_WARNING("RTP receiver is already running with the same handler");
            return;
        }

        if (owner_ != nullptr) {
            RAV_WARNING("RTP receiver is already running");
            return;
        }

        owner_ = &owner;

        async_wait_rtp();
        async_wait_rtcp();

        RAV_TRACE(
            "RTP Receiver impl started. RTP on {}:{}, RTCP on {}:{}",
            rtp_socket_.local_endpoint().address().to_string(), rtp_socket_.local_endpoint().port(),
            rtcp_socket_.local_endpoint().address().to_string(), rtcp_socket_.local_endpoint().port()
        );
    }

    void stop() {
        owner_ = nullptr;

        // (No need to call shutdown on the sockets as they are datagram sockets).

        asio::error_code ec;
        rtp_socket_.close(ec);
        if (ec) {
            RAV_ERROR("Failed to close RTP socket: {}", ec.message());
        }
        rtcp_socket_.close(ec);
        if (ec) {
            RAV_ERROR("Failed to close RTCP socket: {}", ec.message());
        }

        RAV_TRACE("Endpoint stopped.");
    }

    void join_multicast_group(const std::string& multicast_address, const std::string& interface_address) {
        rtp_socket_.set_option(asio::ip::multicast::join_group(
            asio::ip::make_address(multicast_address).to_v4(), asio::ip::make_address(interface_address).to_v4()
        ));

        rtcp_socket_.set_option(asio::ip::multicast::join_group(
            asio::ip::make_address(multicast_address).to_v4(), asio::ip::make_address(interface_address).to_v4()
        ));
    }

  private:
    rtp_receiver* owner_ = nullptr;
    asio::ip::udp::socket rtp_socket_;
    asio::ip::udp::socket rtcp_socket_;
    asio::ip::udp::endpoint rtp_sender_endpoint_;   // For receiving the senders address.
    asio::ip::udp::endpoint rtcp_sender_endpoint_;  // For receiving the senders address.
    std::array<uint8_t, 1500> rtp_data_ {};
    std::array<uint8_t, 1500> rtcp_data_ {};

    void async_wait_rtp() {
        auto self = shared_from_this();
        rtp_socket_.async_wait(asio::socket_base::wait_read, [self](std::error_code ec) {
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
            RAV_TRACE("RTP Socket ready to read.");
            while (self->rtp_socket_.available() > 0) {
                const auto bytes_received = receive_from_socket(self->rtp_socket_, self->rtp_data_, ec);
                if (ec) {
                    RAV_ERROR("Read error: {}. Closing connection.", ec.message());
                    return;
                }
                if (bytes_received == 0) {
                    break;
                }
                const rtp_packet_view rtp_packet(self->rtp_data_.data(), bytes_received);
                if (rtp_packet.validate()) {
                    const rtp_packet_event event {rtp_packet};
                    self->owner_->on(event);
                } else {
                    RAV_WARNING("Invalid RTP packet received. Ignoring.");
                }
            }
            self->async_wait_rtp();  // Schedule another round of waiting.
        });
    }

    void async_wait_rtcp() {
        auto self = shared_from_this();
        rtp_socket_.async_wait(asio::socket_base::wait_read, [self](const std::error_code& ec) {
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
            RAV_TRACE("RTCP Socket ready to read.");
        });
    }

    static size_t
    receive_from_socket(asio::ip::udp::socket& socket, std::array<uint8_t, 1500>& data_buf, asio::error_code& ec) {
        sockaddr_in src_addr {};
        iovec iov[1];
        char ctrl_buf[CMSG_SPACE(sizeof(in_addr))];
        msghdr msg {};

        iov[0].iov_base = data_buf.data();
        iov[0].iov_len = data_buf.size();

        msg.msg_name = &src_addr;
        msg.msg_namelen = sizeof(src_addr);
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;
        msg.msg_control = ctrl_buf;
        msg.msg_controllen = sizeof(ctrl_buf);
        msg.msg_flags = 0;

        const ssize_t received_bytes = recvmsg(socket.native_handle(), &msg, 0);
        if (received_bytes < 0) {
            ec = asio::error_code(errno, asio::system_category());
            return 0;
        }

        // Extract the destination IP from the control message
        for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
            if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_RECVDSTADDR_PKTINFO) {
                const auto* dst_addr = reinterpret_cast<struct in_addr*>(CMSG_DATA(cmsg));
                char dest_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, dst_addr, dest_ip, sizeof(dest_ip));
                RAV_TRACE("Received packet destined to: {}", dest_ip);
            }
        }

        return received_bytes;
    }
};

rav::rtp_receiver::rtp_receiver(asio::io_context& io_context) : io_context_(io_context) {}

rav::rtp_receiver::~rtp_receiver() {
    stop();
}

void rav::rtp_receiver::start(const asio::ip::address& bind_addr, uint16_t rtp_port, uint16_t rtcp_port) {
    if (impl_) {
        RAV_WARNING("RTP receiver already running");
        return;
    }
    impl_ = std::make_shared<impl>(io_context_, bind_addr, rtp_port, rtcp_port);
    impl_->start(*this);

    RAV_TRACE(
        "RTP Receiver started. RTP on {}:{}, RTCP on {}:{}", bind_addr.to_string(), rtp_port, bind_addr.to_string(),
        rtcp_port
    );
}

void rav::rtp_receiver::stop() {
    if (impl_ != nullptr) {
        impl_->stop();
        impl_.reset();
        RAV_TRACE("RTP Receiver stopped.");
    }
}

void rav::rtp_receiver::join_multicast_group(
    const std::string& multicast_address, const std::string& interface_address
) {
    if (impl_ == nullptr) {
        RAV_ERROR("RTP receiver is not running");
        return;
    }
    impl_->join_multicast_group(multicast_address, interface_address);
}

void rav::rtp_receiver::subscribe(subscriber& subscriber) {
    subscribers_.add(&subscriber);
}

void rav::rtp_receiver::unsubscribe(subscriber& subscriber) {
    subscribers_.remove(&subscriber);
}

void rav::rtp_receiver::on(const rtp_packet_event& rtp_event) {
    RAV_INFO("{}", rtp_event.packet.to_string());
}

void rav::rtp_receiver::on(const rtcp_packet_event& rtcp_event) {
    RAV_INFO("{}", rtcp_event.packet.to_string());
}
