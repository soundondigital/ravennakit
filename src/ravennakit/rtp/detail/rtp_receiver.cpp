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
#include "ravennakit/core/util/subscriber_list.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/core/util/tracy.hpp"
#include "ravennakit/core/util/defer.hpp"
#include "ravennakit/core/util/tracy.hpp"

#include <fmt/core.h>
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

[[nodiscard]] bool setup_socket(boost::asio::ip::udp::socket& socket, const uint16_t port) {
    const auto endpoint = boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4::any(), port);

    try {
        socket.open(boost::asio::ip::udp::v4());
        socket.set_option(boost::asio::ip::udp::socket::reuse_address(true));
        socket.bind(endpoint);
        socket.non_blocking(true);
        socket.set_option(boost::asio::detail::socket_option::integer<IPPROTO_IP, IP_RECVDSTADDR_PKTINFO>(1));
        RAV_TRACE("Opened socket for port {}", port);
    } catch (const std::exception& e) {
        RAV_ERROR("Failed to setup receive socket: {}", e.what());
        socket.close();
        return false;
    }

    return true;
}

[[nodiscard]] boost::asio::ip::udp::socket* find_socket(rav::rtp::Receiver3& receiver, const uint16_t port) {
    // Try to find existing socket
    for (auto& ctx : receiver.sockets) {
        if (ctx.port == port) {
            return &ctx.socket;
        }
    }
    return nullptr;
}

[[nodiscard]] boost::asio::ip::udp::socket*
find_or_create_socket(rav::rtp::Receiver3& receiver, const uint16_t port, boost::asio::io_context& io_context) {
    // Try to find existing socket
    if (auto* found = find_socket(receiver, port)) {
        return found;
    }

    // Try to reuse existing socket slot
    for (auto& ctx : receiver.sockets) {
        const auto socket_guard = ctx.rw_lock.lock_exclusive();
        if (!socket_guard) {
            return nullptr;
        }

        if (ctx.socket.is_open()) {
            continue;  // Slot not available, try next one
        }

        if (!setup_socket(ctx.socket, port)) {
            return nullptr;
        }
        RAV_ASSERT(ctx.socket.is_open(), "Socket expected to be open at this point");
        ctx.port = port;
        return &ctx.socket;
    }

    // Try to add new socket
    if (receiver.sockets.size() >= decltype(receiver.sockets)::max_size()) {
        return nullptr;  // No space left
    }

    auto& it = receiver.sockets.emplace_back(io_context);
    if (!setup_socket(it.socket, port)) {
        return nullptr;
    }
    RAV_ASSERT(it.socket.is_open(), "Socket expected to be open at this point");
    it.port = port;
    return &it.socket;
}

[[nodiscard]] uint32_t count_multicast_groups(
    rav::rtp::Receiver3& receiver, const boost::asio::ip::address_v4& multicast_group,
    const boost::asio::ip::address_v4& interface_address, const uint16_t port
) {
    RAV_ASSERT(multicast_group.is_multicast(), "Multicast group should be a multicast address");
    RAV_ASSERT(!interface_address.is_unspecified(), "Interface address should not be unspecified");
    RAV_ASSERT(!interface_address.is_multicast(), "Interface address should not be a multicast address");
    RAV_ASSERT(port > 0, "Port must be a non-zero value");

    uint32_t total = 0;
    for (auto& reader : receiver.readers) {
        for (size_t i = 0; i < reader.sessions.size(); ++i) {
            if (reader.interfaces[i] != interface_address) {
                continue;
            }
            if (reader.sessions[i].connection_address == multicast_group && reader.sessions[i].rtp_port == port) {
                total++;
            }
        }
    }
    return total;
}

[[nodiscard]] bool setup_reader(
    rav::rtp::Receiver3& receiver, rav::rtp::Receiver3::Reader& reader, const rav::Id id,
    const rav::rtp::Receiver3::ArrayOfSessions& sessions, const rav::rtp::Receiver3::ArrayOfFilters& filters,
    const rav::rtp::Receiver3::ArrayOfAddresses& interfaces, boost::asio::io_context& io_context
) {
    reader.id = id;
    reader.sessions = sessions;
    reader.filters = filters;
    reader.interfaces = interfaces;
    reader.fifo.consume_all([](const auto&) {});
    reader.receive_buffer.clear();

    for (size_t i = 0; i < reader.sessions.size(); ++i) {
        auto& session = reader.sessions[i];
        if (!session.valid()) {
            continue;
        }
        const auto endpoint = boost::asio::ip::udp::endpoint(session.connection_address, session.rtp_port);

        auto* socket = find_or_create_socket(receiver, endpoint.port(), io_context);
        if (socket == nullptr) {
            RAV_ERROR("Failed to create receive socket");
            continue;
        }

        auto& interface_address = reader.interfaces[i];
        if (session.connection_address.is_multicast()) {
            if (!interface_address.is_unspecified()) {
                const auto count = count_multicast_groups(
                    receiver, session.connection_address.to_v4(), interface_address, session.rtp_port
                );
                // 1 because the reader being set up is also counted
                if (count == 1) {
                    if (!receiver.join_multicast_group(
                            *socket, session.connection_address.to_v4(), interface_address
                        )) {
                        RAV_ERROR("Failed to join multicast group");
                    }
                }
            }
        }
    }

    return true;
}

/// Leaves given multicast group if last.
[[nodiscard]] bool leave_multicast_group_if_last(
    rav::rtp::Receiver3& receiver, const boost::asio::ip::address_v4& multicast_address,
    const boost::asio::ip::address_v4& interface_address, const uint16_t port
) {
    RAV_ASSERT(multicast_address.is_multicast(), "Expecting multicast address to be a multicast address");
    RAV_ASSERT(!multicast_address.is_unspecified(), "Expected multicast address to not be unspecified");
    RAV_ASSERT(!interface_address.is_multicast(), "Expected interface address to not be a multicast address");
    RAV_ASSERT(!interface_address.is_unspecified(), "Expecting interface address to not be unspecified");

    const auto count = count_multicast_groups(receiver, multicast_address, interface_address, port);
    if (count != 1) {
        return false;
    }

    for (auto& socket : receiver.sockets) {
        if (socket.port == port) {
            RAV_ASSERT(socket.socket.is_open(), "Socket is not open");
            // Note: not locking the rw_lock here as joining and leaving a multicast group should be thread safe, and we
            // don't want to interrupt the call to read_incoming_packets().
            if (!receiver.leave_multicast_group(socket.socket, multicast_address, interface_address)) {
                RAV_ERROR(
                    "Failed to leave multicast group {}:{} on {}", multicast_address.to_string(), port,
                    interface_address.to_string()
                );
            }
        }
    }

    return true;
}

size_t count_num_sessions_using_rtp_port(rav::rtp::Receiver3& receiver, const uint16_t port) {
    RAV_ASSERT(port > 0, "A valid port must be given, otherwise empty sessions will be counted as well");
    size_t count = 0;
    for (auto& reader : receiver.readers) {
        for (const auto& session : reader.sessions) {
            if (port == session.rtp_port) {
                count++;
            }
        }
    }
    return count;
}

void close_unused_sockets(rav::rtp::Receiver3& receiver) {
    for (auto& socket : receiver.sockets) {
        if (!socket.socket.is_open()) {
            continue;
        }
        if (count_num_sessions_using_rtp_port(receiver, socket.port) == 0) {
            boost::system::error_code ec;
            socket.socket.close(ec);
            if (ec) {
                RAV_ERROR("Failed to close socket for port {}", socket.port);
            } else {
                RAV_TRACE("Closed socket for port {}", socket.port);
            }
        }
    }
}

}  // namespace

rav::rtp::Receiver3::Receiver3() {
    join_multicast_group = [](boost::asio::ip::udp::socket& socket, const boost::asio::ip::address_v4& multicast_group,
                              const boost::asio::ip::address_v4& interface_address) {
        RAV_ASSERT(multicast_group.is_multicast(), "Multicast group should be a multicast address");
        RAV_ASSERT(!interface_address.is_unspecified(), "Interface address should not be unspecified");
        RAV_ASSERT(!interface_address.is_multicast(), "Interface address should not be a multicast address");

        boost::system::error_code ec;
        socket.set_option(boost::asio::ip::multicast::join_group(multicast_group, interface_address), ec);
        if (ec) {
            RAV_ERROR("Failed to join multicast group: {}", ec.message());
            return false;
        }
        RAV_TRACE(
            "Joined multicast group {}:{} on {}", multicast_group.to_string(), socket.local_endpoint().port(),
            interface_address.to_string()
        );
        return true;
    };

    leave_multicast_group = [](boost::asio::ip::udp::socket& socket, const boost::asio::ip::address_v4& multicast_group,
                               const boost::asio::ip::address_v4& interface_address) {
        RAV_ASSERT(multicast_group.is_multicast(), "Multicast group should be a multicast address");
        RAV_ASSERT(!interface_address.is_unspecified(), "Interface address should not be unspecified");
        RAV_ASSERT(!interface_address.is_multicast(), "Interface address should not be a multicast address");

        boost::system::error_code ec;
        socket.set_option(boost::asio::ip::multicast::leave_group(multicast_group, interface_address), ec);
        if (ec) {
            RAV_ERROR("Failed to leave multicast group: {}", ec.message());
            return false;
        }
        RAV_TRACE(
            "Left multicast group {}:{} on {}", multicast_group.to_string(), socket.local_endpoint().port(),
            interface_address.to_string()
        );
        return true;
    };
}

rav::rtp::Receiver3::~Receiver3() {
    for (auto& reader : readers) {
        RAV_ASSERT(!reader.id.is_valid(), "There should be no active readers at this point");
    }
    for (auto& socket : sockets) {
        RAV_ASSERT(!socket.socket.is_open(), "There should be no active socket at this point");
    }
}

void rav::rtp::Receiver3::set_interfaces(const ArrayOfAddresses& interfaces) {
    for (auto& reader : readers) {
        for (size_t i = 0; i < interfaces.size(); i++) {
            if (reader.interfaces[i] == interfaces[i]) {
                continue;
            }
            if (!reader.interfaces[i].is_unspecified()) {
                if (reader.sessions[i].connection_address.is_multicast()) {
                    std::ignore = leave_multicast_group_if_last(
                        *this, reader.sessions[i].connection_address.to_v4(), reader.interfaces[i],
                        reader.sessions[i].rtp_port
                    );
                }
            }
            reader.interfaces[i] = {};
            if (!interfaces[i].is_unspecified()) {
                if (reader.sessions[i].connection_address.is_multicast()) {
                    const auto count = count_multicast_groups(
                        *this, reader.sessions[i].connection_address.to_v4(), interfaces[i], reader.sessions[i].rtp_port
                    );
                    if (count == 0) {
                        auto* socket = find_socket(*this, reader.sessions[i].rtp_port);
                        RAV_ASSERT(socket != nullptr, "Socket not found");
                        if (!join_multicast_group(
                                *socket, reader.sessions[i].connection_address.to_v4(), interfaces[i]
                            )) {
                            RAV_ERROR("Failed to join multicast group");
                        }
                    }
                }
            }
            reader.interfaces[i] = interfaces[i];
        }
    }

    close_unused_sockets(*this);
}

bool rav::rtp::Receiver3::add_reader(
    const Id id, const ArrayOfSessions& sessions, const ArrayOfFilters& filters, const ArrayOfAddresses& interfaces,
    boost::asio::io_context& io_context
) {
    RAV_ASSERT(sessions.size() == filters.size(), "Should be equal");
    RAV_ASSERT(sessions.size() == interfaces.size(), "Should be equal");

    for (auto& stream : readers) {
        if (stream.id == id) {
            RAV_WARNING("A stream for given id already exists");
            return false;  // Stream already exists
        }
    }

    for (auto& reader : readers) {
        const auto guard = reader.rw_lock.lock_exclusive();
        if (!guard) {
            RAV_ERROR("Failed to exclusively lock reader");
            return false;
        }

        if (reader.id.is_valid()) {
            continue;
        }

        return setup_reader(*this, reader, id, sessions, filters, interfaces, io_context);
    }

    if (readers.size() >= decltype(readers)::max_size()) {
        RAV_TRACE("No space available for stream");
        return false;  // No space left
    }

    // Open a new reader slot
    auto& reader = readers.emplace_back();
    const auto guard = reader.rw_lock.lock_exclusive();
    if (!guard) {
        RAV_ERROR("Failed to exclusively lock reader");
        return false;
    }

    return setup_reader(*this, reader, id, sessions, filters, interfaces, io_context);
}

bool rav::rtp::Receiver3::remove_reader(const Id id) {
    for (auto& e : readers) {
        if (e.id == id) {
            const auto guard = e.rw_lock.lock_exclusive();
            if (!guard) {
                RAV_ERROR("Failed to exclusively lock reader");
                return false;
            }

            for (size_t i = 0; i < e.sessions.size(); ++i) {
                auto& session = e.sessions[i];
                if (session.valid()) {
                    if (session.connection_address.is_multicast() && !e.interfaces[i].is_unspecified()) {
                        std::ignore = leave_multicast_group_if_last(
                            *this, session.connection_address.to_v4(), e.interfaces[i], session.rtp_port
                        );
                    }
                }
            }

            e.id = {};
            e.sessions = {};
            e.filters = {};
            e.fifo.consume_all([](const auto&) {});
            e.receive_buffer.clear();

            close_unused_sockets(*this);

            return true;
        }
    }
    return false;
}

void rav::rtp::Receiver3::read_incoming_packets() {
    TRACY_ZONE_SCOPED;

    for (auto& ctx : sockets) {
        const auto socket_guard = ctx.rw_lock.try_lock_shared();
        if (!socket_guard) {
            continue;  // Exclusively locked, so it either just appeared or is going away.
        }

        if (!ctx.socket.is_open()) {
            continue;  // This means unused. I think the call is stable and will not be changed externally.
        }

        if (!ctx.socket.available()) {
            continue;
        }

        std::array<uint8_t, aes67::constants::k_mtu> receive_buffer {};
        boost::asio::ip::udp::endpoint src_endpoint;
        boost::asio::ip::udp::endpoint dst_endpoint;
        uint64_t recv_time = 0;
        boost::system::error_code ec;
        const auto bytes_received =
            receive_from_socket(ctx.socket, receive_buffer, src_endpoint, dst_endpoint, recv_time, ec);

        if (ec == boost::asio::error::try_again) {
            continue;
        }

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

        for (auto& reader : readers) {
            const auto reader_guard = reader.rw_lock.try_lock_shared();
            if (!reader_guard) {
                continue;  // Failed to lock which means it is being added or removed.
            }

            if (!reader.id.is_valid()) {
                continue;
            }

            bool valid_source = false;
            for (size_t i = 0; i < reader.sessions.size(); ++i) {
                if (reader.sessions[i].connection_address == dst_endpoint.address()
                    && reader.sessions[i].rtp_port == dst_endpoint.port()) {
                    if (reader.filters[i].is_valid_source(dst_endpoint.address(), src_endpoint.address())) {
                        valid_source = true;
                        break;
                    }
                }
            }

            if (!valid_source) {
                continue;
            }

            std::array<uint8_t, aes67::constants::k_mtu> packet {};
            std::memcpy(packet.data(), receive_buffer.data(), bytes_received);

            if (!reader.fifo.push(packet)) {
                RAV_ERROR("Failed to push data to fifo");
            }
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
