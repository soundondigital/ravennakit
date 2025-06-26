/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/net/sockets/extended_udp_socket.hpp"

#include "ravennakit/core/util/tracy.hpp"
#include "ravennakit/core/chrono/high_resolution_clock.hpp"
#include "ravennakit/core/platform/windows/wsa_recv_msg_function.hpp"
#include "ravennakit/core/platform/windows/qos_flow.hpp"

#if RAV_APPLE
    #define IP_RECVDSTADDR_PKTINFO IP_RECVDSTADDR
#else
    #define IP_RECVDSTADDR_PKTINFO IP_PKTINFO
#endif

namespace {

#if RAV_WINDOWS
size_t receive_from_socket(
    boost::asio::ip::udp::socket& socket, std::array<uint8_t, 1500>& data_buf, boost::asio::ip::udp::endpoint& src_endpoint,
    boost::asio::ip::udp::endpoint& dst_endpoint, uint64_t& recv_time, boost::system::error_code& ec
) {
    TRACY_ZONE_SCOPED;
    // Set up the message structure
    char control_buf[1024];
    WSABUF wsabuf;
    wsabuf.buf = reinterpret_cast<char*>(data_buf.data());
    wsabuf.len = static_cast<ULONG>(data_buf.size());

    sockaddr src_addr {};

    WSAMSG msg;
    memset(&msg, 0, sizeof(msg));
    msg.name = &src_addr;
    msg.namelen = sizeof(src_addr);
    msg.lpBuffers = &wsabuf;
    msg.dwBufferCount = 1;
    msg.Control.len = sizeof(control_buf);
    msg.Control.buf = control_buf;

    DWORD bytes_received = 0;
    auto wsa_recv_msg = rav::windows::wsa_recv_msg_function::get_global();
    if (wsa_recv_msg(socket.native_handle(), &msg, &bytes_received, nullptr, nullptr) == SOCKET_ERROR) {
        ec = boost::system::error_code(WSAGetLastError(), boost::system::system_category());
        return 0;
    }
    recv_time = rav::HighResolutionClock::now();

    if (src_addr.sa_family == AF_INET) {
        const auto* addr_in = reinterpret_cast<const sockaddr_in*>(&src_addr);
        src_endpoint =
            boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4(ntohl(addr_in->sin_addr.s_addr)), ntohs(addr_in->sin_port));
    }

    // Process control messages to get the destination IP
    for (WSACMSGHDR* cmsg = WSA_CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = WSA_CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
            auto* pktinfo = reinterpret_cast<IN_PKTINFO*>(WSA_CMSG_DATA(cmsg));
            IN_ADDR dest_addr = pktinfo->ipi_addr;

            dst_endpoint = boost::asio::ip::udp::endpoint(
                boost::asio::ip::address_v4(ntohl(dest_addr.s_addr)), socket.local_endpoint(ec).port()
            );

            if (ec) {
                RAV_ERROR("Failed to get port from local endpoint");
            }

            char dest_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &dest_addr, dest_ip, sizeof(dest_ip));
            break;
        }
    }

    return bytes_received;
}
#else
size_t receive_from_socket(
    boost::asio::ip::udp::socket& socket, std::array<uint8_t, 1500>& data_buf, boost::asio::ip::udp::endpoint& src_endpoint,
    boost::asio::ip::udp::endpoint& dst_endpoint, uint64_t& recv_time, boost::system::error_code& ec
) {
    TRACY_ZONE_SCOPED;
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
    recv_time = rav::HighResolutionClock::now();
    if (received_bytes < 0) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return 0;
    }

    // Extract the destination IP from the control message
    for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_RECVDSTADDR_PKTINFO) {
            const auto* dst_addr = reinterpret_cast<struct in_addr*>(CMSG_DATA(cmsg));
            dst_endpoint =
                boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4(ntohl(dst_addr->s_addr)), ntohs(src_addr.sin_port));
        }
    }

    src_endpoint =
        boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4(ntohl(src_addr.sin_addr.s_addr)), ntohs(src_addr.sin_port));

    return static_cast<size_t>(received_bytes);
}
#endif

}  // namespace

class rav::ExtendedUdpSocket::Impl: public std::enable_shared_from_this<Impl> {
  public:
    explicit Impl(boost::asio::io_context& io_context, const boost::asio::ip::udp::endpoint& endpoint);

    void start(HandlerType handler);
    void stop();

    void async_receive();
    void send(const uint8_t* data, size_t size, const boost::asio::ip::udp::endpoint& endpoint);

    boost::system::error_code
    join_multicast_group(const boost::asio::ip::address& multicast_address, const boost::asio::ip::address& interface_address);

    boost::system::error_code
    leave_multicast_group(const boost::asio::ip::address_v4& multicast_address, const boost::asio::ip::address_v4& interface_address);

    [[nodiscard]] boost::system::error_code set_multicast_outbound_interface(const boost::asio::ip::address_v4& interface_address);

    boost::system::error_code set_multicast_loopback(bool enable);

    void set_dscp_value(int value);

  private:
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint sender_endpoint_ {};  // For receiving the senders address.
    std::array<uint8_t, 1500> recv_data_ {};
    HandlerType handler_;

#if RAV_WINDOWS
    qos_flow qos_flow_;
#endif
};

void rav::ExtendedUdpSocket::Impl::set_dscp_value(const int value) {
#if RAV_WINDOWS
    if (!qos_flow_.has_socket()) {
        if (!qos_flow_.add_socket_to_flow(socket_)) {
            RAV_ERROR("Failed to add socket to flow");
            return;
        }
    }

    if (!qos_flow_.set_dscp_value(value)) {
        RAV_ERROR("Failed to set DSCP value on flow");
        return;
    }
#else
    // Note: this does not work on Windows, unfortunately. It's a whole other story to get this going on Windows...
    socket_.set_option(boost::asio::detail::socket_option::integer<IPPROTO_IP, IP_TOS>(value << 2));
#endif
}

boost::system::error_code rav::ExtendedUdpSocket::Impl::set_multicast_loopback(const bool enable) {
    boost::system::error_code ec;
    socket_.set_option(boost::asio::ip::multicast::enable_loopback(enable), ec);
    return ec;
}

boost::system::error_code
rav::ExtendedUdpSocket::Impl::set_multicast_outbound_interface(const boost::asio::ip::address_v4& interface_address) {
    boost::system::error_code ec;
    socket_.set_option(boost::asio::ip::multicast::outbound_interface(interface_address), ec);
    return ec;
}

void rav::ExtendedUdpSocket::Impl::send(
    const uint8_t* data, const size_t size, const boost::asio::ip::udp::endpoint& endpoint
) {
    RAV_ASSERT(data != nullptr, "Data must not be null");
    RAV_ASSERT(size > 0, "Size must be greater than 0");
    boost::system::error_code ec;
    const auto sent = socket_.send_to(boost::asio::buffer(data, size), endpoint, 0, ec);
    if (ec) {
        RAV_ERROR("Failed to send data: {}", ec.message());
        return;
    }
    if (sent != size) {
        RAV_ERROR("Failed to send all data");
        return;
    }
}

void rav::ExtendedUdpSocket::Impl::stop() {
    if (handler_ == nullptr) {
        return;  // Nothing to do here
    }

    handler_ = nullptr;

    // (No need to call shutdown on the sockets as they are datagram sockets).

    boost::system::error_code ec;
    socket_.close(ec);
    if (ec) {
        RAV_ERROR("Failed to close socket: {}", ec.message());
    }

    RAV_TRACE("Stopped extended udp socket");
}

void rav::ExtendedUdpSocket::Impl::start(HandlerType handler) {
    if (handler_) {
        RAV_WARNING("RTP receiver is already running");
        return;
    }

    handler_ = std::move(handler);

    async_receive();

    RAV_TRACE(
        "Started extended udp socket on {}:{}", socket_.local_endpoint().address().to_string(),
        socket_.local_endpoint().port()
    );
}

rav::ExtendedUdpSocket::Impl::Impl(boost::asio::io_context& io_context, const boost::asio::ip::udp::endpoint& endpoint) :
    socket_(io_context) {
    socket_.open(endpoint.protocol());
    socket_.set_option(boost::asio::ip::udp::socket::reuse_address(true));
    socket_.bind(endpoint);
    socket_.non_blocking(true);
    socket_.set_option(boost::asio::detail::socket_option::integer<IPPROTO_IP, IP_RECVDSTADDR_PKTINFO>(1));
}

void rav::ExtendedUdpSocket::Impl::async_receive() {
    auto self = shared_from_this();
    socket_.async_wait(boost::asio::socket_base::wait_read, [self](boost::system::error_code ec) {
        TRACY_ZONE_SCOPED;
        if (ec) {
            if (ec == boost::asio::error::operation_aborted) {
                RAV_TRACE("Operation aborted");
                return;
            }
            if (ec == boost::asio::error::eof) {
                RAV_TRACE("EOF");
                return;
            }
            RAV_ERROR("Read error: {}. Closing connection.", ec.message());
            return;
        }

        if (self->handler_ == nullptr) {
            RAV_TRACE("No handler. Closing connection.");
            return;
        }

        for (int i = 0; i < 10; ++i) {
            const auto available = self->socket_.available(ec);

            if (ec) {
                RAV_ERROR("Read error: {}. Closing connection.", ec.message());
                return;
            }

            if (available == 0) {
                break;
            }

            boost::asio::ip::udp::endpoint src_endpoint;
            boost::asio::ip::udp::endpoint dst_endpoint;
            uint64_t recv_time = 0;
            const auto bytes_received =
                receive_from_socket(self->socket_, self->recv_data_, src_endpoint, dst_endpoint, recv_time, ec);

            if (ec) {
                RAV_ERROR("Read error: {}. Closing connection.", ec.message());
                return;
            }
            if (bytes_received == 0) {
                break;
            }

            if (self->handler_) {
                self->handler_({self->recv_data_.data(), bytes_received, src_endpoint, dst_endpoint, recv_time});
            } else {
                RAV_ERROR("No handler available. Closing connection.");
                return;
            }
        }
        self->async_receive();  // Schedule another round.
    });
}

boost::system::error_code rav::ExtendedUdpSocket::Impl::join_multicast_group(
    const boost::asio::ip::address& multicast_address, const boost::asio::ip::address& interface_address
) {
    boost::system::error_code ec;
    socket_.set_option(boost::asio::ip::multicast::join_group(multicast_address.to_v4(), interface_address.to_v4()), ec);

    if (ec) {
        RAV_ERROR("Failed to join multicast group: {}", ec.message());
        return ec;
    }

    const auto local_endpoint = socket_.local_endpoint(ec);
    if (ec) {
        RAV_ERROR("Failed to get local endpoint: {}", ec.message());
        return ec;
    }

    RAV_TRACE(
        "Joined multicast group: {} on {}:{}", multicast_address.to_string(), interface_address.to_string(),
        local_endpoint.port()
    );

    return {};
}

boost::system::error_code rav::ExtendedUdpSocket::Impl::leave_multicast_group(
    const boost::asio::ip::address_v4& multicast_address, const boost::asio::ip::address_v4& interface_address
) {
    boost::system::error_code ec;
    socket_.set_option(boost::asio::ip::multicast::leave_group(multicast_address, interface_address), ec);
    if (ec) {
        RAV_ERROR("Failed to leave multicast group: {}", ec.message());
        return ec;
    }

    const auto local_endpoint = socket_.local_endpoint(ec);
    if (ec) {
        RAV_ERROR("Failed to get local endpoint: {}", ec.message());
        return ec;
    }

    RAV_TRACE(
        "Left multicast group: {} on {}:{}", multicast_address.to_string(), interface_address.to_string(),
        local_endpoint.port()
    );

    return {};
}

void rav::ExtendedUdpSocket::start(HandlerType handler) const {
    TRACY_ZONE_SCOPED;
    impl_->start(std::move(handler));
}

void rav::ExtendedUdpSocket::send(
    const uint8_t* data, const size_t size, const boost::asio::ip::udp::endpoint& endpoint
) const {
    impl_->send(data, size, endpoint);
}

boost::system::error_code rav::ExtendedUdpSocket::join_multicast_group(
    const boost::asio::ip::address_v4& multicast_address, const boost::asio::ip::address_v4& interface_address
) const {
    if (impl_ == nullptr) {
        RAV_WARNING("No implementation available");
        return {};
    }
    return impl_->join_multicast_group(multicast_address, interface_address);
}

boost::system::error_code rav::ExtendedUdpSocket::leave_multicast_group(
    const boost::asio::ip::address_v4& multicast_address, const boost::asio::ip::address_v4& interface_address
) const {
    if (impl_ == nullptr) {
        RAV_WARNING("No implementation available");
        return {};
    }
    return impl_->leave_multicast_group(multicast_address, interface_address);
}

boost::system::error_code
rav::ExtendedUdpSocket::set_multicast_outbound_interface(const boost::asio::ip::address_v4& interface_address) const {
    if (impl_ == nullptr) {
        RAV_WARNING("No implementation available");
        return {};
    }
    return impl_->set_multicast_outbound_interface(interface_address);
}

boost::system::error_code rav::ExtendedUdpSocket::set_multicast_loopback(const bool enable) const {
    if (impl_ == nullptr) {
        RAV_WARNING("No implementation available");
        return {};
    }
    return impl_->set_multicast_loopback(enable);
}

rav::ExtendedUdpSocket::ExtendedUdpSocket(boost::asio::io_context& io_context, const boost::asio::ip::udp::endpoint& endpoint) :
    impl_(std::make_shared<Impl>(io_context, endpoint)) {}

rav::ExtendedUdpSocket::ExtendedUdpSocket(
    boost::asio::io_context& io_context, const boost::asio::ip::address& interface_address, const uint16_t port
) :
    ExtendedUdpSocket(io_context, boost::asio::ip::udp::endpoint(interface_address, port)) {}

rav::ExtendedUdpSocket::~ExtendedUdpSocket() {
    if (impl_) {
        impl_->stop();
        impl_.reset();
    }
}

void rav::ExtendedUdpSocket::set_dscp_value(const int value) const {
    if (impl_) {
        impl_->set_dscp_value(value);
    }
}
