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
#include "ravennakit/rtp/detail/rtp_receiver.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/audio/audio_data.hpp"
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

[[nodiscard]] boost::asio::ip::udp::socket* find_or_create_socket(rav::rtp::Receiver3& receiver, const uint16_t port) {
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

    return nullptr;
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
        for (auto& stream : reader.streams) {
            if (stream.interface != interface_address) {
                continue;
            }
            if (stream.session.connection_address != multicast_group) {
                continue;
            }
            if (stream.session.rtp_port != port) {
                continue;
            }
            total++;
        }
    }
    return total;
}

[[nodiscard]] bool setup_reader(
    rav::rtp::Receiver3& receiver, rav::rtp::Receiver3::Reader& reader, const rav::Id id,
    const rav::rtp::Receiver3::ReaderParameters& parameters, const rav::rtp::Receiver3::ArrayOfAddresses& interfaces
) {
    RAV_ASSERT(parameters.streams.size() == interfaces.size(), "Unequal size");
    RAV_ASSERT(parameters.audio_format.is_valid(), "Invalid format");

    reader.id = id;

    for (size_t i = 0; i < reader.streams.size(); ++i) {
        reader.streams[i].reset();
        reader.streams[i].session = parameters.streams[i].session;
        reader.streams[i].filter = parameters.streams[i].filter;
        reader.streams[i].packet_time_frames = parameters.streams[i].packet_time_frames;
        reader.streams[i].interface = interfaces[i];
    }

    reader.audio_format = parameters.audio_format;
    reader.receive_buffer.clear();

    // Find the smallest packet time frames
    uint16_t packet_time_frames = std::numeric_limits<uint16_t>::max();
    for (const auto& stream : parameters.streams) {
        if (stream.packet_time_frames > 0 && stream.packet_time_frames < packet_time_frames) {
            packet_time_frames = stream.packet_time_frames;
        }
    }

    const auto bytes_per_frame = reader.audio_format.bytes_per_frame();
    RAV_ASSERT(bytes_per_frame > 0, "bytes_per_frame must be greater than 0");

    const auto buffer_size_frames =
        std::max(reader.audio_format.sample_rate * rav::rtp::Receiver3::k_buffer_size_ms / 1000, 1024u);
    reader.receive_buffer.resize(
        reader.audio_format.sample_rate * rav::rtp::Receiver3::k_buffer_size_ms / 1000, bytes_per_frame
    );

    reader.read_audio_data_buffer.resize(buffer_size_frames * bytes_per_frame);
    const auto buffer_size_packets = buffer_size_frames / packet_time_frames;

    for (auto& stream : reader.streams) {
        if (!stream.session.valid()) {
            stream.packets.reset();
            stream.packets_too_old.reset();
            continue;
        }

        stream.packets.resize(buffer_size_packets);
        stream.packets_too_old.resize(buffer_size_packets);

        const auto endpoint =
            boost::asio::ip::udp::endpoint(stream.session.connection_address, stream.session.rtp_port);

        auto* socket = find_or_create_socket(receiver, endpoint.port());
        if (socket == nullptr) {
            RAV_ERROR("Failed to create receive socket");
            continue;
        }

        if (stream.session.connection_address.is_multicast()) {
            if (!stream.interface.is_unspecified()) {
                const auto count = count_multicast_groups(
                    receiver, stream.session.connection_address.to_v4(), stream.interface, stream.session.rtp_port
                );
                if (count == 1) {  // 1 because the current reader being set up is also counted
                    if (!receiver
                             .join_multicast_group(*socket, stream.session.connection_address.to_v4(), stream.interface)) {
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
        for (const auto& stream : reader.streams) {
            if (stream.session.rtp_port == port) {
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

void do_realtime_maintenance(rav::rtp::Receiver3::Reader& reader) {
    TRACY_ZONE_SCOPED;

    RAV_ASSERT(reader.rw_lock.is_locked_shared(), "Reader must be shared locked");

    if (reader.consumer_active_.exchange(true) == false) {
        for (auto& stream : reader.streams) {
            stream.packets.pop_all();
            stream.packets_too_old.pop_all();
        }
        return;
    }

    for (auto& stream : reader.streams) {
        for (size_t i = 0; i < stream.packets.size(); ++i) {
            auto rtp_packet = stream.packets.pop();
            if (!rtp_packet.has_value()) {
                break;
            }

            rav::rtp::PacketView rtp_packet_view(rtp_packet->data(), rtp_packet->size());

            const auto seq = rtp_packet_view.sequence_number();
            const auto ts = rtp_packet_view.timestamp();

            rav::WrappingUint32 packet_timestamp(ts);
            if (!reader.first_packet_timestamp) {
                RAV_TRACE("First packet timestamp: {}", packet_timestamp.value());
                reader.first_packet_timestamp = packet_timestamp;
                reader.receive_buffer.set_next_ts(packet_timestamp.value());
                reader.next_ts = packet_timestamp - 480;  // TODO: Fix the hardcoding
            }

            // Determine whether whole packet is too old
            if (packet_timestamp + stream.packet_time_frames <= reader.next_ts) {
                // RAV_WARNING("Packet too late: seq={}, ts={}", packet->seq, packet->timestamp);
                TRACY_MESSAGE("Packet too late - skipping");
                if (!stream.packets_too_old.push(rtp_packet_view.sequence_number())) {
                    // RAV_ERROR("Packet not enqueued to packets_too_old");
                }
                continue;
            }

            // Determine whether packet contains outdated data
            if (packet_timestamp < reader.next_ts) {
                RAV_WARNING("Packet partly too late: seq={}, ts={}", seq, ts);
                TRACY_MESSAGE("Packet partly too late - not skipping");
                if (!stream.packets_too_old.push(seq)) {
                    RAV_ERROR("Packet not enqueued to packets_too_old");
                }
                // Still process the packet since it contains data that is not outdated
            }

            reader.receive_buffer.clear_until(ts);

            if (!reader.receive_buffer.write(ts, rtp_packet_view.payload_data())) {
                RAV_ERROR("Packet not written to buffer");
            }
        }
    }

    TRACY_PLOT("available_frames", static_cast<int64_t>(reader.next_ts.diff(reader.receive_buffer.get_next_ts())));
}

std::optional<uint32_t> read_data_realtime_from_reader(
    rav::rtp::Receiver3::Reader& reader, uint8_t* buffer, const size_t buffer_size,
    const std::optional<uint32_t> at_timestamp
) {
    TRACY_ZONE_SCOPED;

    do_realtime_maintenance(reader);

    if (at_timestamp.has_value()) {
        reader.next_ts = *at_timestamp;
    } else if (!reader.first_packet_timestamp.has_value()) {
        return {};
    }

    const auto num_frames = static_cast<uint32_t>(buffer_size) / reader.audio_format.bytes_per_frame();

    const auto read_at = reader.next_ts.value();
    if (!reader.receive_buffer.read(read_at, buffer, buffer_size, true)) {
        TRACY_MESSAGE("Failed to read data from ringbuffer");
        return std::nullopt;
    }

    reader.next_ts += num_frames;

    return read_at;
}

}  // namespace

void rav::rtp::Receiver3::StreamContext::reset() {
    session = {};
    filter = {};
    interface = {};
    packets.reset();
    packets_too_old.reset();
}

rav::rtp::Receiver3::Receiver3(boost::asio::io_context& io_context) {
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

    for (size_t i = sockets.size(); i < sockets.capacity(); i++) {
        sockets.emplace_back(io_context);
    }

    for (size_t i = readers.size(); i < readers.capacity(); i++) {
        readers.emplace_back();
    }
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
        RAV_ASSERT(interfaces.size() == reader.streams.size(), "Size mismatch");

        // TODO: Should we lock here?
        for (size_t i = 0; i < reader.streams.size(); i++) {
            if (reader.streams[i].interface == interfaces[i]) {
                continue;
            }
            if (!reader.streams[i].interface.is_unspecified()) {
                if (reader.streams[i].session.connection_address.is_multicast()) {
                    std::ignore = leave_multicast_group_if_last(
                        *this, reader.streams[i].session.connection_address.to_v4(), reader.streams[i].interface,
                        reader.streams[i].session.rtp_port
                    );
                }
            }
            reader.streams[i].interface = {};
            if (!interfaces[i].is_unspecified()) {
                if (reader.streams[i].session.connection_address.is_multicast()) {
                    const auto count = count_multicast_groups(
                        *this, reader.streams[i].session.connection_address.to_v4(), interfaces[i],
                        reader.streams[i].session.rtp_port
                    );
                    if (count == 0) {
                        auto* socket = find_socket(*this, reader.streams[i].session.rtp_port);
                        RAV_ASSERT(socket != nullptr, "Socket not found");
                        if (!join_multicast_group(
                                *socket, reader.streams[i].session.connection_address.to_v4(), interfaces[i]
                            )) {
                            RAV_ERROR("Failed to join multicast group");
                        }
                    }
                }
            }
            reader.streams[i].interface = interfaces[i];
        }
    }

    close_unused_sockets(*this);
}

bool rav::rtp::Receiver3::add_reader(
    const Id id, const ReaderParameters& parameters, const ArrayOfAddresses& interfaces
) {
    RAV_ASSERT(parameters.streams.size() == interfaces.size(), "Should be equal");

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
            continue;  // Used already
        }

        return setup_reader(*this, reader, id, parameters, interfaces);
    }

    return false;
}

bool rav::rtp::Receiver3::remove_reader(const Id id) {
    for (auto& e : readers) {
        if (e.id == id) {
            const auto guard = e.rw_lock.lock_exclusive();
            if (!guard) {
                RAV_ERROR("Failed to exclusively lock reader");
                return false;
            }

            for (auto& stream : e.streams) {
                if (stream.session.valid() && stream.session.connection_address.is_multicast()
                    && !stream.interface.is_unspecified()) {
                    std::ignore = leave_multicast_group_if_last(
                        *this, stream.session.connection_address.to_v4(), stream.interface, stream.session.rtp_port
                    );
                }
                stream.reset();
            }

            e.id = {};
            e.receive_buffer.clear();
            // TODO: Should we reset more fields here?

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

            for (auto& stream : reader.streams) {
                if (stream.session.connection_address != dst_endpoint.address()) {
                    continue;
                }
                if (stream.session.rtp_port != dst_endpoint.port()) {
                    continue;
                }
                if (!stream.filter.is_valid_source(dst_endpoint.address(), src_endpoint.address())) {
                    continue;
                }

                PacketBuffer packet;
                std::memcpy(packet.data(), receive_buffer.data(), bytes_received);

                if (!stream.packets.push(packet)) {
                    // RAV_ERROR("Failed to push data to fifo");
                    RAV_ERROR("Push: {}", view.to_string());
                    TRACY_MESSAGE("Failed to push packet");
                    reader.consumer_active_ = false;
                    // TODO: Make sure consumer_active change is notified
                }
            }
        }
    }
}

std::optional<uint32_t> rav::rtp::Receiver3::read_data_realtime(
    const Id id, uint8_t* buffer, const size_t buffer_size, const std::optional<uint32_t> at_timestamp
) {
    TRACY_ZONE_SCOPED;

    for (auto& reader : readers) {
        const auto guard = reader.rw_lock.try_lock_shared();
        if (!guard) {
            TRACY_MESSAGE("Failed to lock reader");
            continue;
        }
        if (reader.id != id) {
            continue;
        }
        return read_data_realtime_from_reader(reader, buffer, buffer_size, at_timestamp);
    }

    return std::nullopt;
}

std::optional<uint32_t> rav::rtp::Receiver3::read_audio_data_realtime(
    const Id id, AudioBufferView<float> output_buffer, const std::optional<uint32_t> at_timestamp
) {
    TRACY_ZONE_SCOPED;

    RAV_ASSERT(output_buffer.is_valid(), "Buffer must be valid");

    for (auto& reader : readers) {
        const auto guard = reader.rw_lock.try_lock_shared();
        if (!guard) {
            continue;
        }
        if (reader.id != id) {
            continue;
        }

        const auto format = reader.audio_format;

        if (format.byte_order != AudioFormat::ByteOrder::be) {
            RAV_ERROR("Unexpected byte order");
            return std::nullopt;
        }

        if (format.ordering != AudioFormat::ChannelOrdering::interleaved) {
            RAV_ERROR("Unexpected channel ordering");
            return std::nullopt;
        }

        if (format.num_channels != output_buffer.num_channels()) {
            RAV_ERROR("Channel mismatch");
            return std::nullopt;
        }

        auto& buffer = reader.read_audio_data_buffer;
        const auto read_at = read_data_realtime_from_reader(
            reader, buffer.data(), output_buffer.num_frames() * format.bytes_per_frame(), at_timestamp
        );

        if (!read_at.has_value()) {
            return std::nullopt;
        }

        if (format.encoding == AudioEncoding::pcm_s16) {
            const auto ok = AudioData::convert<
                int16_t, AudioData::ByteOrder::Be, AudioData::Interleaving::Interleaved, float,
                AudioData::ByteOrder::Ne>(
                reinterpret_cast<int16_t*>(buffer.data()), output_buffer.num_frames(), output_buffer.num_channels(),
                output_buffer.data()
            );
            if (!ok) {
                RAV_WARNING("Failed to convert audio data");
            }
        } else if (format.encoding == AudioEncoding::pcm_s24) {
            const auto ok = AudioData::convert<
                int24_t, AudioData::ByteOrder::Be, AudioData::Interleaving::Interleaved, float,
                AudioData::ByteOrder::Ne>(
                reinterpret_cast<int24_t*>(buffer.data()), output_buffer.num_frames(), output_buffer.num_channels(),
                output_buffer.data()
            );
            if (!ok) {
                RAV_WARNING("Failed to convert audio data");
            }
        } else {
            RAV_ERROR("Unsupported encoding");
            return std::nullopt;
        }

        return read_at;
    }

    return std::nullopt;
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
