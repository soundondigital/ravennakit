/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/detail/rtp_audio_receiver.hpp"

#include "ravennakit/core/clock.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/audio/audio_data.hpp"
#include "ravennakit/core/net/sockets/extended_udp_socket.hpp"
#include "ravennakit/rtp/rtcp_packet_view.hpp"
#include "ravennakit/core/util/subscriber_list.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/core/util/tracy.hpp"
#include "ravennakit/core/util/defer.hpp"

#include <fmt/core.h>
#include <utility>

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

[[nodiscard]] boost::asio::ip::udp::socket* find_socket(rav::rtp::AudioReceiver& receiver, const uint16_t port) {
    // Try to find existing socket
    for (auto& ctx : receiver.sockets) {
        if (ctx.port == port) {
            return &ctx.socket;
        }
    }
    return nullptr;
}

[[nodiscard]] boost::asio::ip::udp::socket*
find_or_create_socket(rav::rtp::AudioReceiver& receiver, const uint16_t port) {
    RAV_ASSERT(port > 0, "Port should be non zero");

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
    rav::rtp::AudioReceiver& receiver, const boost::asio::ip::address_v4& multicast_group,
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

void reset_stream_context(rav::rtp::AudioReceiver::StreamContext& stream) {
    stream.session = {};
    stream.filter = {};
    stream.interface = {};
    stream.rtp_ts = {};
    stream.packets.reset();
    stream.packets_too_old.reset();
    stream.packet_stats.reset();
    stream.packet_stats_counters.write({});
    stream.packet_interval_stats = {};
    stream.prev_packet_time_ns = {};
}

void reset_reader(rav::rtp::AudioReceiver::Reader& reader) {
    reader.id = {};
    reader.audio_format = {};
    for (auto& stream : reader.streams) {
        reset_stream_context(stream);
    }
    reader.receive_buffer.clear();
    reader.read_audio_data_buffer = {};
    reader.most_recent_ts = {};
    reader.next_ts_to_read = {};
}

[[nodiscard]] bool setup_reader(
    rav::rtp::AudioReceiver& receiver, rav::rtp::AudioReceiver::Reader& reader, const rav::Id id,
    const rav::rtp::AudioReceiver::ReaderParameters& parameters,
    const rav::rtp::AudioReceiver::ArrayOfAddresses& interfaces
) {
    RAV_ASSERT(parameters.streams.size() == interfaces.size(), "Unequal size");
    RAV_ASSERT(parameters.audio_format.is_valid(), "Invalid format");
    RAV_ASSERT(reader.rw_lock.is_locked_exclusively(), "Expecting the reader to be locked exclusively");

    reader.id = id;

    for (size_t i = 0; i < reader.streams.size(); ++i) {
        reset_stream_context(reader.streams[i]);
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
        std::max(reader.audio_format.sample_rate * rav::rtp::AudioReceiver::k_buffer_size_ms / 1000, 1024u);
    reader.receive_buffer.resize(
        reader.audio_format.sample_rate * rav::rtp::AudioReceiver::k_buffer_size_ms / 1000, bytes_per_frame
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
    rav::rtp::AudioReceiver& receiver, const boost::asio::ip::address_v4& multicast_address,
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

size_t count_num_sessions_using_rtp_port(rav::rtp::AudioReceiver& receiver, const uint16_t port) {
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

void close_unused_sockets(rav::rtp::AudioReceiver& receiver) {
    for (auto& socket : receiver.sockets) {
        if (!socket.socket.is_open()) {
            continue;
        }
        if (count_num_sessions_using_rtp_port(receiver, socket.port) == 0) {
            // We're only locking here which is safe as long as the audio thread and network thread only take shared
            // locks. Once that is not the case and these threads start to manipulate the socket structure there will be
            // a data race.
            const auto guard = socket.rw_lock.lock_exclusive();
            if (!guard) {
                RAV_ERROR("Failed to lock socket, cannot close");
                continue;
            }
            boost::system::error_code ec;
            socket.socket.close(ec);
            if (ec) {
                RAV_ERROR("Failed to close socket for port {}", socket.port);
            } else {
                RAV_TRACE("Closed socket for port {}", socket.port);
            }
            socket.port = {};
        }
    }
}

void do_realtime_maintenance(rav::rtp::AudioReceiver::Reader& reader) {
    TRACY_ZONE_SCOPED;

    RAV_ASSERT(reader.rw_lock.is_locked_shared(), "Reader must be shared locked");

    for (auto& stream : reader.streams) {
        if (stream.state.load(std::memory_order_relaxed) == rav::rtp::AudioReceiver::StreamState::no_consumer) {
            stream.packets.pop_all();
            stream.packets_too_old.pop_all();
            continue;
        }

        for (size_t i = 0; i < stream.packets.size(); ++i) {
            auto rtp_packet = stream.packets.pop();
            if (!rtp_packet.has_value()) {
                break;
            }

            rav::WrappingUint32 packet_timestamp(rtp_packet->timestamp);
            const auto num_frames = static_cast<uint32_t>(rtp_packet->data_len) / reader.audio_format.bytes_per_frame();
            auto packet_most_recent_ts = rav::WrappingUint32(rtp_packet->timestamp + num_frames - 1);

            if (!reader.most_recent_ts.has_value()) {
                RAV_TRACE("First packet at: {}", packet_timestamp.value());
                reader.most_recent_ts = packet_most_recent_ts;
                reader.receive_buffer.set_next_ts(packet_timestamp.value());
                reader.next_ts_to_read = packet_timestamp;
            }

            if (packet_most_recent_ts > *reader.most_recent_ts) {
                reader.most_recent_ts = packet_most_recent_ts;
            }

            TRACY_PLOT("Packet margin: {}", static_cast<double>(reader.next_ts_to_read.diff(packet_timestamp)));

            // Determine whether whole packet is too old
            if (packet_timestamp + stream.packet_time_frames <= reader.next_ts_to_read) {
                // RAV_WARNING("Packet too late: seq={}, ts={}", packet->seq, packet->timestamp);
                TRACY_MESSAGE("Packet too late - skipping");
                if (!stream.packets_too_old.push(rtp_packet->seq)) {
                    RAV_ERROR("Packet not enqueued to packets_too_old");
                }
                continue;
            }

            // Determine whether part of the packet is too old
            if (packet_timestamp < reader.next_ts_to_read) {
                RAV_WARNING("Packet partly too late: seq={}, ts={}", rtp_packet->seq, rtp_packet->timestamp);
                TRACY_MESSAGE("Packet partly too late - not skipping");
                if (!stream.packets_too_old.push(rtp_packet->seq)) {
                    RAV_ERROR("Packet not enqueued to packets_too_old");
                }
                // Still process the packet since it contains data that is not outdated
            }

            reader.receive_buffer.clear_until(rtp_packet->timestamp);

            if (!reader.receive_buffer.write(
                    rtp_packet->timestamp, {rtp_packet->payload.data(), rtp_packet->data_len}
                )) {
                RAV_ERROR("Packet not written to buffer");
            }
        }
    }

    TRACY_PLOT(
        "available_frames", static_cast<int64_t>(reader.next_ts_to_read.diff(reader.receive_buffer.get_next_ts()))
    );
}

std::optional<uint32_t> read_data_from_reader_realtime(
    rav::rtp::AudioReceiver::Reader& reader, uint8_t* buffer, const size_t buffer_size,
    const std::optional<uint32_t> at_timestamp, const std::optional<uint32_t> require_delay
) {
    TRACY_ZONE_SCOPED;

    RAV_ASSERT(reader.rw_lock.is_locked_shared(), "Reader must be shared locked");

    if (at_timestamp.has_value()) {
        // Updating before do_realtime_maintenance to have the most accurate next_to_to_read
        reader.next_ts_to_read = *at_timestamp;
    }

    do_realtime_maintenance(reader);

    if (!reader.most_recent_ts.has_value()) {
        return {};  // No data has been received yet
    }

    RAV_ASSERT(reader.most_recent_ts.has_value(), "Should have a value, since first_packet_timestamp is set");

    const auto num_frames = static_cast<uint32_t>(buffer_size) / reader.audio_format.bytes_per_frame();

    if (require_delay.has_value()) {
        if (reader.next_ts_to_read + num_frames - 1 + *require_delay > reader.most_recent_ts) {
            return {};
        }
    }

    const auto read_at = reader.next_ts_to_read.value();
    if (!reader.receive_buffer.read(read_at, buffer, buffer_size, true)) {
        TRACY_MESSAGE("Failed to read data from ringbuffer");
        return std::nullopt;
    }

    reader.next_ts_to_read += num_frames;

    return read_at;
}

void update_stream_active_state(rav::rtp::AudioReceiver::StreamContext& stream, const uint64_t now) {
    TRACY_ZONE_SCOPED;
    if ((stream.prev_packet_time_ns + rav::rtp::AudioReceiver::k_receive_timeout_ms * 1'000'000).value() < now) {
        stream.state.store(rav::rtp::AudioReceiver::StreamState::inactive, std::memory_order_relaxed);
    }
}

}  // namespace

rav::rtp::AudioReceiver::AudioReceiver(boost::asio::io_context& io_context) {
    join_multicast_group = [](boost::asio::ip::udp::socket& socket, const boost::asio::ip::address_v4& multicast_group,
                              const boost::asio::ip::address_v4& interface_address) {
        RAV_ASSERT(socket.is_open(), "Socket should be open");
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
        RAV_ASSERT(socket.is_open(), "Socket should be open");
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

    for (size_t i = sockets.size(); i < decltype(sockets)::capacity(); i++) {
        sockets.emplace_back(io_context);
    }

    for (size_t i = readers.size(); i < decltype(readers)::capacity(); i++) {
        readers.emplace_back();
    }
}

rav::rtp::AudioReceiver::~AudioReceiver() {
    for (auto& reader : readers) {
        RAV_ASSERT_NO_THROW(!reader.id.is_valid(), "There should be no active readers at this point");
    }
    for (auto& socket : sockets) {
        RAV_ASSERT_NO_THROW(!socket.socket.is_open(), "There should be no active socket at this point");
    }
}

bool rav::rtp::AudioReceiver::set_interfaces(const ArrayOfAddresses& interfaces) {
    for (auto& reader : readers) {
        RAV_ASSERT(interfaces.size() == reader.streams.size(), "Size mismatch");

        const auto guard = reader.rw_lock.lock_exclusive();
        if (!guard) {
            RAV_ERROR("Failed to exclusively lock writer");
            return false;
        }

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
    return true;
}

bool rav::rtp::AudioReceiver::add_reader(
    const Id id, const ReaderParameters& parameters, const ArrayOfAddresses& interfaces
) {
    RAV_ASSERT(parameters.streams.size() == interfaces.size(), "Should be equal");

    for (auto& reader : readers) {
        if (reader.id == id) {
            RAV_WARNING("A reader for given id already exists");
            return false;
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

bool rav::rtp::AudioReceiver::remove_reader(const Id id) {
    for (auto& reader : readers) {
        if (reader.id == id) {
            const auto guard = reader.rw_lock.lock_exclusive();
            if (!guard) {
                RAV_ERROR("Failed to exclusively lock reader");
                return false;
            }

            for (auto& stream : reader.streams) {
                if (stream.session.valid() && stream.session.connection_address.is_multicast()
                    && !stream.interface.is_unspecified()) {
                    std::ignore = leave_multicast_group_if_last(
                        *this, stream.session.connection_address.to_v4(), stream.interface, stream.session.rtp_port
                    );
                }
            }

            reset_reader(reader);
            close_unused_sockets(*this);

            return true;
        }
    }
    return false;
}

void rav::rtp::AudioReceiver::read_incoming_packets() {
    TRACY_ZONE_SCOPED;

    const auto now = clock::now_monotonic_high_resolution_ns();

    for (auto& ctx : sockets) {
        const auto socket_guard = ctx.rw_lock.try_lock_shared();
        if (!socket_guard) {
            continue;  // Exclusively locked, so it either just appeared or is going away.
        }

        if (!ctx.socket.is_open()) {
            continue;  // This means unused. I think the call is stable and will not be changed externally.
        }

        boost::system::error_code ec;
        std::array<uint8_t, aes67::constants::k_mtu> receive_buffer {};
        boost::asio::ip::udp::endpoint src_endpoint;
        boost::asio::ip::udp::endpoint dst_endpoint;
        uint64_t recv_time = 0;
        const auto bytes_received =
            receive_from_socket(ctx.socket, receive_buffer, src_endpoint, dst_endpoint, recv_time, ec);

        if (ec == boost::asio::error::try_again) {
            // Normally you would call ctx.socket.available(ec); to test if there is data available, but to safe time we
            // test for boost::asio::error::try_again.
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

        const auto payload = view.payload_data();
        if (payload.size_bytes() == 0) {
            RAV_WARNING("Received packet with empty payload");
            return;
        }

        if (payload.size_bytes() > std::numeric_limits<uint16_t>::max()) {
            RAV_WARNING("Payload size exceeds maximum size");
            return;
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
                update_stream_active_state(stream, now);

                if (stream.session.connection_address != dst_endpoint.address()) {
                    continue;
                }
                if (stream.session.rtp_port != dst_endpoint.port()) {
                    continue;
                }
                if (!stream.filter.is_valid_source(dst_endpoint.address(), src_endpoint.address())) {
                    continue;
                }

                update_stream_active_state(stream, now);

                if (!stream.rtp_ts.has_value()) {
                    stream.rtp_ts = view.timestamp();
                    stream.prev_packet_time_ns = recv_time;
                }

                PacketBuffer packet {};
                packet.timestamp = view.timestamp();
                packet.seq = view.sequence_number();
                packet.data_len = static_cast<uint16_t>(payload.size_bytes());
                std::memcpy(packet.payload.data(), payload.data(), payload.size_bytes());

                auto state = stream.state.load(std::memory_order_relaxed);
                if (state != StreamState::no_consumer && !stream.packets.push(packet)) {
                    stream.state.store(StreamState::no_consumer, std::memory_order_relaxed);
                } else {
                    stream.state.store(StreamState::receiving, std::memory_order_relaxed);
                }

                while (auto seq = stream.packets_too_old.pop()) {
                    stream.packet_stats.mark_packet_too_late(*seq);
                }

                if (const auto interval = stream.prev_packet_time_ns.update(recv_time)) {
                    if (stream.packet_interval_stats.initialized || *interval != 0) {
                        stream.packet_interval_stats.update(static_cast<double>(*interval) / 1'000'000.0);
                        TRACY_PLOT("packet interval (ms)", static_cast<double>(*interval) / 1'000'000.0);
                        TRACY_PLOT("packet interval EMA (ms)", stream.packet_interval_stats.interval);
                    }
                }

                std::ignore = stream.packet_stats.update(view.sequence_number());
                auto stats = stream.packet_stats.get_total_counts();
                stats.jitter = stream.packet_interval_stats.max_deviation;
                stream.packet_stats_counters.write(stats);
            }
        }

        last_time_maintenance = now;
    }

    // Do maintenance if not done for a while
    if (last_time_maintenance + k_receive_timeout_ms * 1'000'000 < now) {
        for (auto& reader : readers) {
            const auto reader_guard = reader.rw_lock.try_lock_shared();
            if (!reader_guard) {
                continue;  // Failed to lock which means it is being added or removed.
            }

            if (!reader.id.is_valid()) {
                continue;
            }

            for (auto& stream : reader.streams) {
                update_stream_active_state(stream, now);
            }
        }
        last_time_maintenance = now;
    }
}

std::optional<uint32_t> rav::rtp::AudioReceiver::read_data_realtime(
    const Id id, uint8_t* buffer, const size_t buffer_size, const std::optional<uint32_t> at_timestamp,
    const std::optional<uint32_t> require_delay
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
        return read_data_from_reader_realtime(reader, buffer, buffer_size, at_timestamp, require_delay);
    }

    return std::nullopt;
}

std::optional<uint32_t> rav::rtp::AudioReceiver::read_audio_data_realtime(
    const Id id, AudioBufferView<float> output_buffer, const std::optional<uint32_t> at_timestamp,
    const std::optional<uint32_t> require_delay
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
            // Channel mapping/mixing is not implemented and an equal amount of channels is expected. Ignoring silently.
            RAV_ERROR("Unexpected number of channels");
            return std::nullopt;
        }

        auto& buffer = reader.read_audio_data_buffer;
        const auto read_at = read_data_from_reader_realtime(
            reader, buffer.data(), output_buffer.num_frames() * format.bytes_per_frame(), at_timestamp, require_delay
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

std::optional<rav::rtp::PacketStats::Counters>
rav::rtp::AudioReceiver::get_packet_stats(const Id reader_id, const size_t stream_index) {
    for (auto& reader : readers) {
        if (reader.id != reader_id) {
            continue;
        }
        const auto guard = reader.rw_lock.try_lock_shared();
        if (!guard) {
            continue;
        }

        if (stream_index >= reader.streams.size()) {
            RAV_ASSERT_FALSE("Index out of bounds");
            return std::nullopt;
        }

        auto& stream = reader.streams[stream_index];
        stream.reset_max_values.store(true, std::memory_order_release);
        return stream.packet_stats_counters.read(boost::lockfree::uses_optional);
    }

    return std::nullopt;
}

std::optional<rav::rtp::AudioReceiver::StreamState>
rav::rtp::AudioReceiver::get_stream_state(const Id reader_id, const size_t stream_index) const {
    for (auto& reader : readers) {
        if (reader.id == reader_id) {
            if (stream_index >= reader.streams.size()) {
                RAV_ASSERT_FALSE("Index out of bounds");
                return {};
            }

            return reader.streams[stream_index].state.load(std::memory_order_relaxed);
        }
    }
    return {};
}

const char* rav::rtp::to_string(const AudioReceiver::StreamState state) {
    switch (state) {
        case AudioReceiver::StreamState::inactive:
            return "inactive";
        case AudioReceiver::StreamState::receiving:
            return "receiving";
        case AudioReceiver::StreamState::no_consumer:
            return "ok_no_consumer";
    }
    return "n/a";
}
