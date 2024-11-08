/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/detail/udp_sender_receiver.hpp"

void rav::udp_sender_receiver::start(handler_type handler) {
    TRACY_ZONE_SCOPED;

    if (handler_) {
        RAV_WARNING("RTP receiver is already running");
        return;
    }

    handler_ = std::move(handler);

    async_receive();

    RAV_TRACE(
        "udp_sender_receiver started on {}:{}", socket_.local_endpoint().address().to_string(),
        socket_.local_endpoint().port()
    );
}

void rav::udp_sender_receiver::stop() {
    if (handler_ == nullptr) {
        return;  // Nothing to do here
    }

    handler_ = nullptr;

    // (No need to call shutdown on the sockets as they are datagram sockets).

    asio::error_code ec;
    socket_.close(ec);
    if (ec) {
        RAV_ERROR("Failed to close socket: {}", ec.message());
    }

    RAV_TRACE("Stopped udp_sender_receiver");
}

void rav::udp_sender_receiver::join_multicast_group(
    const asio::ip::address& multicast_address, const asio::ip::address& interface_address
) {
    socket_.set_option(asio::ip::multicast::join_group(multicast_address.to_v4(), interface_address.to_v4()));
}

rav::udp_sender_receiver::udp_sender_receiver(asio::io_context& io_context, const asio::ip::udp::endpoint& endpoint) :
    socket_(io_context) {
    socket_.open(endpoint.protocol());
    socket_.set_option(asio::ip::udp::socket::reuse_address(true));
    socket_.bind(endpoint);
    socket_.non_blocking(true);
    socket_.set_option(asio::detail::socket_option::integer<IPPROTO_IP, IP_RECVDSTADDR_PKTINFO>(1));

#if RAV_WINDOWS
    load_wsa_recv_msg_func();
#endif
}

rav::udp_sender_receiver::udp_sender_receiver(
    asio::io_context& io_context, const asio::ip::address& interface_address, const uint16_t port
) :
    udp_sender_receiver(io_context, asio::ip::udp::endpoint(interface_address, port)) {}

void rav::udp_sender_receiver::async_receive() {
    auto self = shared_from_this();
    socket_.async_wait(asio::socket_base::wait_read, [self](std::error_code ec) {
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
            RAV_ERROR("No handler. Closing connection.");
            return;
        }

        while (self->socket_.available(ec) > 0) {
            if (ec) {
                RAV_ERROR("Read error: {}. Closing connection.", ec.message());
                return;
            }

            asio::ip::udp::endpoint src_endpoint;
            asio::ip::udp::endpoint dst_endpoint;

            const auto bytes_received =
                receive_from_socket(self->socket_, self->recv_data_, self, src_endpoint, dst_endpoint, ec);

            if (ec) {
                RAV_ERROR("Read error: {}. Closing connection.", ec.message());
                return;
            }
            if (bytes_received == 0) {
                break;
            }

            if (self->handler_) {
                self->handler_(self->recv_data_.data(), bytes_received, src_endpoint, dst_endpoint);
            } else {
                RAV_ERROR("No handler available. Closing connection.");
                return;
            }
        }
        self->async_receive();  // Schedule another round.
    });
}

#if RAV_WINDOWS
size_t rav::udp_sender_receiver::receive_from_socket(
    asio::ip::udp::socket& socket, std::array<uint8_t, 1500>& data_buf,
    const std::shared_ptr<udp_sender_receiver>& self, asio::ip::udp::endpoint& src_endpoint,
    asio::ip::udp::endpoint& dst_endpoint, asio::error_code& ec
) {
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
    if (self->wsa_recv_msg_func(socket.native_handle(), &msg, &bytes_received, nullptr, nullptr) == SOCKET_ERROR) {
        ec = asio::error_code(WSAGetLastError(), asio::system_category());
        return 0;
    }

    if (src_addr.sa_family == AF_INET) {
        const auto* addr_in = reinterpret_cast<const sockaddr_in*>(&src_addr);
        src_endpoint =
            asio::ip::udp::endpoint(asio::ip::address_v4(ntohl(addr_in->sin_addr.s_addr)), ntohs(addr_in->sin_port));
    }

    // Process control messages to get the destination IP
    for (WSACMSGHDR* cmsg = WSA_CMSG_FIRSTHDR(&msg); cmsg != nullptr; cmsg = WSA_CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_PKTINFO) {
            auto* pktinfo = reinterpret_cast<IN_PKTINFO*>(WSA_CMSG_DATA(cmsg));
            IN_ADDR dest_addr = pktinfo->ipi_addr;

            dst_endpoint = asio::ip::udp::endpoint(
                asio::ip::address_v4(ntohl(dest_addr.s_addr)), socket.local_endpoint(ec).port()
            );

            if (ec) {
                RAV_ERROR("Failed to get port from local endpoint");
            }

            char dest_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &dest_addr, dest_ip, sizeof(dest_ip));
            std::cout << "Received packet destined to: " << dest_ip << std::endl;
            break;
        }
    }

    RAV_TRACE(
        "Received from {}:{} to {}:{}", src_endpoint.address().to_string(), src_endpoint.port(),
        dst_endpoint.address().to_string(), dst_endpoint.port()
    );

    return bytes_received;
}
#else
size_t rav::udp_sender_receiver::receive_from_socket(
    asio::ip::udp::socket& socket, std::array<uint8_t, 1500>& data_buf,
    const std::shared_ptr<udp_sender_receiver>& self, asio::ip::udp::endpoint& src_endpoint,
    asio::ip::udp::endpoint& dst_endpoint, asio::error_code& ec
) {
    std::ignore = self;
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
            dst_endpoint =
                asio::ip::udp::endpoint(asio::ip::address_v4(ntohl(dst_addr->s_addr)), ntohs(src_addr.sin_port));
        }
    }

    src_endpoint =
        asio::ip::udp::endpoint(asio::ip::address_v4(ntohl(src_addr.sin_addr.s_addr)), ntohs(src_addr.sin_port));

    return static_cast<size_t>(received_bytes);
}
#endif
