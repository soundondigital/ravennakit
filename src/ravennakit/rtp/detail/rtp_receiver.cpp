/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/detail/rtp_receiver.hpp"

#include "ravennakit/core/log.hpp"
#include "ravennakit/rtp/rtcp_packet_view.hpp"
#include "ravennakit/rtp/detail/rtp_receiver.hpp"

#include "ravennakit/core/util/subscriber_list.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/core/util/tracy.hpp"

#include <fmt/core.h>
#include "ravennakit/core/expected.hpp"

#include <utility>

#if RAV_APPLE
    #define IP_RECVDSTADDR_PKTINFO IP_RECVDSTADDR
#else
    #define IP_RECVDSTADDR_PKTINFO IP_PKTINFO
#endif

#if RAV_WINDOWS
typedef BOOL(PASCAL* LPFN_WSARECVMSG)(
    SOCKET s, LPWSAMSG lpMsg, LPDWORD lpNumberOfBytesRecvd, LPWSAOVERLAPPED lpOverlapped,
    LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine
);
#endif

namespace {

bool has_socket(
    rav::rtp::Receiver3& receiver, const boost::asio::ip::udp::endpoint& connection_endpoint,
    const boost::asio::ip::address_v4& interface_address
) {
    for (auto& ctx : receiver.sockets) {
        if (ctx.connection_endpoint == connection_endpoint && ctx.interface_address == interface_address) {
            return true;
        }
    }
    return false;
}

bool setup_socket(
    boost::asio::ip::udp::socket& socket, const boost::asio::ip::udp::endpoint& connection_endpoint,
    const boost::asio::ip::address_v4& interface_address
) {
    auto interface_endpoint = boost::asio::ip::udp::endpoint(interface_address, connection_endpoint.port());

#if !RAV_WINDOWS
    if (connection_endpoint.address().is_multicast()) {
        interface_endpoint.address(connection_endpoint.address());  // Windows cannot bind to a multicast address
    }
#endif

    try {
        socket.open(boost::asio::ip::udp::v4());
        socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
        socket.bind(interface_endpoint);
        socket.non_blocking(true);
        socket.set_option(boost::asio::detail::socket_option::integer<IPPROTO_IP, IP_RECVDSTADDR_PKTINFO>(1));

        if (connection_endpoint.address().is_multicast()) {
            socket.set_option(
                boost::asio::ip::multicast::join_group(connection_endpoint.address().to_v4(), interface_address)
            );
        }
    } catch (const std::exception& e) {
        RAV_ERROR("Failed to setup receive socket: {}", e.what());
        socket.close();
        return false;
    }

    return true;
}

bool add_socket(
    rav::rtp::Receiver3& receiver, const boost::asio::ip::udp::endpoint& endpoint,
    const boost::asio::ip::address_v4& interface_address, boost::asio::io_context& io_context
) {
    for (auto& ctx : receiver.sockets) {
        if (ctx.state.load(std::memory_order_acquire) != rav::rtp::Receiver3::State::available) {
            continue;  // Slot not available, try next one
        }
        ctx.connection_endpoint = endpoint;
        ctx.interface_address = interface_address;
        if (!setup_socket(ctx.socket, ctx.connection_endpoint, ctx.interface_address)) {
            return false;
        }
        ctx.state.store(rav::rtp::Receiver3::State::ready, std::memory_order_release);
    }

    if (receiver.sockets.size() >= decltype(receiver.sockets)::max_size()) {
        return false;  // No space left
    }

    auto& it = receiver.sockets.emplace_back(io_context);
    it.connection_endpoint = endpoint;
    it.interface_address = interface_address;
    if (!setup_socket(it.socket, it.connection_endpoint, it.interface_address)) {
        return false;
    }
    it.state.store(rav::rtp::Receiver3::State::ready, std::memory_order_release);
    return true;
}

}  // namespace

bool rav::rtp::Receiver3::add_stream(
    const Id id, const ArrayOfSessions& sessions, const ArrayOfFilters& filters,
    const ArrayOfAddresses& interface_addresses, boost::asio::io_context& io_context
) {
    RAV_ASSERT(sessions.size() == filters.size(), "Sessions and filters should have an equal amount");
    RAV_ASSERT(
        sessions.size() == interface_addresses.size(), "Sessions and interface addresses should have an equal amount"
    );

    for (auto& stream : streams) {
        if (stream.associated_id == id) {
            RAV_WARNING("A stream for given id already exists");
            return false;  // Stream already exists
        }
    }

    RedundantStream* stream = nullptr;

    for (auto& e : streams) {
        if (e.state.load(std::memory_order_acquire) != State::available) {
            continue;  // Not available to use
        }
        stream = &e;
        break;
    }

    if (stream == nullptr) {
        if (streams.size() >= decltype(streams)::max_size()) {
            RAV_TRACE("No space available for stream");
            return false;  // No space left
        }
        stream = &streams.emplace_back();
    }

    stream->associated_id = id;
    stream->sessions = sessions;
    stream->filters = filters;
    while (stream->fifo.pop()) {}  // Clear the fifo
    stream->receive_buffer.clear();

    for (size_t i = 0; i < stream->sessions.size(); ++i) {
        if (!stream->sessions[i].valid()) {
            continue;
        }
        if (interface_addresses[i].is_multicast()) {
            RAV_ERROR("Interface address should not be multicast");
            continue;
        }
        if (interface_addresses[i].is_unspecified()) {
            RAV_ERROR("Interface address should not be unspecified");
            continue;  // No valid interface address
        }
        const auto endpoint =
            boost::asio::ip::udp::endpoint(stream->sessions[i].connection_address, stream->sessions[i].rtp_port);
        if (!has_socket(*this, endpoint, interface_addresses[i])) {
            add_socket(*this, endpoint, interface_addresses[i], io_context);
        }
    }
    stream->state.store(State::ready, std::memory_order_release);
    return true;
}

void rav::rtp::Receiver3::do_high_prio_processing() {
    for (auto& ctx : sockets) {
        std::array<uint8_t, 1500> receive_buffer {};

        if (ctx.state.load(std::memory_order_acquire) != State::ready) {
            continue;
        }

        if (!ctx.socket.is_open()) {
            continue;
        }

        if (!ctx.socket.available()) {
            continue;
        }

        boost::asio::ip::udp::endpoint src_endpoint;
        boost::asio::ip::udp::endpoint dst_endpoint;
        uint64_t recv_time = 0;
        boost::system::error_code ec;
        const auto bytes_received =
            receive_from_socket(ctx.socket, receive_buffer, src_endpoint, dst_endpoint, recv_time, ec);

        if (ec) {
            RAV_ERROR("Failed to receive from socket: {}", ec.message());
            continue;
        }

        if (bytes_received == 0) {
            continue;
        }

        PacketView view(receive_buffer.data(), bytes_received);
        if (!view.validate()) {
            continue;  // Invalid RTP packet
        }

        for (auto& stream : streams) {
            if (stream.state.load(std::memory_order_acquire) != State::ready) {
                continue;  // Slot not ready for processing
            };

            bool valid_source = false;
            for (size_t i = 0; i < stream.sessions.size(); ++i) {
                if (stream.sessions[i].connection_address == dst_endpoint.address() && stream.sessions[i].rtp_port) {
                    if (stream.filters[i].is_valid_source(dst_endpoint.address(), src_endpoint.address())) {
                        valid_source = true;
                        break;
                    }
                }
            }

            if (!valid_source) {
                continue;
            }

            std::array<uint8_t, aes67::constants::k_max_mtu> packet {};
            std::memcpy(packet.data(), receive_buffer.data(), bytes_received);

            fmt::println(
                "{}:{} => {}:{}", src_endpoint.address().to_string(), src_endpoint.port(),
                dst_endpoint.address().to_string(), dst_endpoint.port()
            );

            if (!stream.fifo.push(packet)) {
                RAV_ERROR("Failed to push data to fifo");
            }
        }
    }

    for (auto& ctx : sockets) {
        if (ctx.state.load(std::memory_order_acquire) == State::should_be_closed) {
            ctx.state.store(State::ready_to_be_closed, std::memory_order_release);
        }
    }
}

rav::rtp::Receiver::Receiver(UdpReceiver& udp_receiver) : udp_receiver_(udp_receiver) {}

bool rav::rtp::Receiver::subscribe(
    Subscriber* subscriber, const Session& session, const boost::asio::ip::address_v4& interface_address
) {
    auto* context = find_or_create_session_context(session, interface_address);

    if (context == nullptr) {
        RAV_WARNING("Failed to find or create new session context");
        return false;
    }

    RAV_ASSERT(context != nullptr, "Expecting valid session at this point");

    return context->add_subscriber(subscriber);
}

bool rav::rtp::Receiver::unsubscribe(const Subscriber* subscriber) {
    size_t count = 0;
    for (auto it = sessions_contexts_.begin(); it != sessions_contexts_.end();) {
        if ((*it)->remove_subscriber(subscriber)) {
            count++;
        }
        if ((*it)->get_subscriber_count() == 0) {
            it = sessions_contexts_.erase(it);
        } else {
            ++it;
        }
    }
    return count > 0;
}

rav::rtp::Receiver::SessionContext::SessionContext(
    UdpReceiver& udp_receiver, Session session, boost::asio::ip::address_v4 interface_address
) :
    udp_receiver_(udp_receiver), session_(std::move(session)), interface_address_(std::move(interface_address)) {
    RAV_ASSERT(!session_.connection_address.is_unspecified(), "Connection address should not be unspecified");
    RAV_ASSERT(!interface_address_.is_unspecified(), "Interface address should not be unspecified");
    RAV_ASSERT(!interface_address_.is_multicast(), "Interface address should not be multicast");
    subscribe_to_udp_receiver(interface_address_);
}

rav::rtp::Receiver::SessionContext::~SessionContext() {
    udp_receiver_.unsubscribe(this);
}

bool rav::rtp::Receiver::SessionContext::add_subscriber(Receiver::Subscriber* subscriber) {
    return subscribers_.add(subscriber);
}

bool rav::rtp::Receiver::SessionContext::remove_subscriber(const Receiver::Subscriber* subscriber) {
    return subscribers_.remove(subscriber);
}

size_t rav::rtp::Receiver::SessionContext::get_subscriber_count() const {
    return subscribers_.size();
}

const rav::rtp::Session& rav::rtp::Receiver::SessionContext::get_session() const {
    return session_;
}

const boost::asio::ip::address_v4& rav::rtp::Receiver::SessionContext::interface_address() const {
    return interface_address_;
}

void rav::rtp::Receiver::SessionContext::on_receive(const ExtendedUdpSocket::RecvEvent& event) {
    if (event.dst_endpoint.port() == session_.rtp_port) {
        handle_incoming_rtp_data(event);
    } else if (event.dst_endpoint.port() == session_.rtcp_port) {
        // TODO: Handle RTCP data
    } else {
        RAV_WARNING("Received data on unknown port: {}", event.dst_endpoint.port());
    }
}

void rav::rtp::Receiver::SessionContext::handle_incoming_rtp_data(const ExtendedUdpSocket::RecvEvent& event) {
    TRACY_ZONE_SCOPED;

    const PacketView packet(event.data, event.size);
    if (!packet.validate()) {
        RAV_WARNING("Invalid RTP packet received");
        return;
    }

    const RtpPacketEvent rtp_event {packet, session_, event.src_endpoint, event.dst_endpoint, event.recv_time};

    bool did_find_stream = false;

    for (const auto& ssrc : synchronization_sources_) {
        if (ssrc == packet.ssrc()) {
            did_find_stream = true;
        }
    }

    if (!did_find_stream) {
        auto ssrc = synchronization_sources_.emplace_back(packet.ssrc());
        RAV_TRACE(
            "Added new stream with SSRC {} from {}:{}", ssrc, event.src_endpoint.address().to_string(),
            event.src_endpoint.port()
        );
    }

    for (auto* s : subscribers_) {
        s->on_rtp_packet(rtp_event);
    }
}

void rav::rtp::Receiver::SessionContext::subscribe_to_udp_receiver(
    const boost::asio::ip::address_v4& interface_address
) {
    if (session_.connection_address.is_multicast()) {
        if (!udp_receiver_.subscribe(this, session_.connection_address.to_v4(), interface_address, session_.rtp_port)) {
            RAV_ERROR("Failed to subscribe to multicast RTP session {}", session_.to_string());
        }
        if (!udp_receiver_.subscribe(
                this, session_.connection_address.to_v4(), interface_address, session_.rtcp_port
            )) {
            RAV_ERROR("Failed to subscribe to multicast RTP session {}", session_.to_string());
        }
    } else {
        if (!udp_receiver_.subscribe(this, interface_address, session_.rtp_port)) {
            RAV_ERROR("Failed to subscribe to unicast RTP session {}", session_.to_string());
        }
        if (!udp_receiver_.subscribe(this, interface_address, session_.rtcp_port)) {
            RAV_ERROR("Failed to subscribe to unicast RTP session {}", session_.to_string());
        }
    }
}

rav::rtp::Receiver::SessionContext* rav::rtp::Receiver::find_session_context(
    const Session& session, const boost::asio::ip::address_v4& interface_address
) const {
    for (const auto& context : sessions_contexts_) {
        if (context->get_session() == session && context->interface_address() == interface_address) {
            return context.get();
        }
    }
    return nullptr;
}

rav::rtp::Receiver::SessionContext* rav::rtp::Receiver::create_new_session_context(
    const Session& session, const boost::asio::ip::address_v4& interface_address
) {
    // TODO: Disallow a port to be used in multiple sessions because when receiving RTP data we don't know which session
    // it belongs to.

    auto new_session = std::make_unique<SessionContext>(udp_receiver_, session, interface_address);
    const auto& it = sessions_contexts_.emplace_back(std::move(new_session));
    RAV_TRACE("New RTP session context created for: {}", session.to_string());
    return it.get();
}

rav::rtp::Receiver::SessionContext* rav::rtp::Receiver::find_or_create_session_context(
    const Session& session, const boost::asio::ip::address_v4& interface_address
) {
    auto context = find_session_context(session, interface_address);
    if (context == nullptr) {
        context = create_new_session_context(session, interface_address);
    }
    return context;
}
