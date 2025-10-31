/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/detail/rtp_audio_sender.hpp"
#include "ravennakit/rtp/detail/rtp_audio_sender.hpp"

#include "ravennakit/core/random.hpp"
#include "ravennakit/core/audio/audio_data.hpp"
#include "ravennakit/core/util/stl_helpers.hpp"
#include "ravennakit/core/util/todo.hpp"
#include "ravennakit/core/util/tracy.hpp"

namespace {

boost::system::error_code setup_socket(rav::udp_socket& socket) {
    boost::system::error_code ec;
    socket.open(boost::asio::ip::udp::v4(), ec);
    if (ec) {
        return ec;
    }
    socket.set_option(boost::asio::ip::multicast::enable_loopback(false), ec);
    if (ec) {
        return ec;
    }
    socket.set_option(boost::asio::ip::udp::socket::reuse_address(true), ec);
    if (ec) {
        return ec;
    }
    return {};
}

bool set_socket_ttl(rav::udp_socket& socket, const uint8_t ttl) {
    boost::system::error_code ec;
    socket.set_option(boost::asio::ip::unicast::hops(ttl), ec);
    if (ec) {
        RAV_LOG_ERROR("Failed to set unicast ttl: {}", ec.message());
        return false;
    }
    socket.set_option(boost::asio::ip::multicast::hops(ttl), ec);
    if (ec) {
        RAV_LOG_ERROR("Failed to set multicast ttl: {}", ec.message());
        return false;
    }
    return true;
}

bool setup_writer(
    rav::rtp::AudioSender::Writer& writer, const rav::Id id, const rav::rtp::AudioSender::WriterParameters& parameters,
    const rav::rtp::AudioSender::ArrayOfAddresses& interfaces
) {
    RAV_ASSERT(writer.rw_lock.is_locked_exclusively(), "Expecting the writer to be locked exclusively");
    RAV_ASSERT(interfaces.size() == writer.sockets.size(), "Unequal size");

    for (size_t i = 0; i < writer.sockets.size(); ++i) {
        if (!writer.sockets[i].is_open()) {
            if (const auto ec = setup_socket(writer.sockets[i])) {
                RAV_LOG_ERROR("Failed to open socket for sending: {}", ec.message());
                return false;
            }
        }
        boost::system::error_code ec;
        writer.sockets[i].set_option(boost::asio::ip::multicast::outbound_interface(interfaces[i]), ec);
        if (ec) {
            RAV_LOG_ERROR("Failed to set outbound interface: {}", ec.message());
            return false;
        }
        if (!set_socket_ttl(writer.sockets[i], parameters.ttl)) {
            return false;
        }
    }

    // TODO: Implement proper SSRC generation (RAV-1)
    const auto ssrc = static_cast<uint32_t>(rav::Random().get_random_int(0, std::numeric_limits<int>::max()));

    const auto audio_format = parameters.audio_format;
    const auto packet_size_bytes = parameters.packet_time_frames * audio_format.bytes_per_frame();
    writer.audio_format = audio_format;
    writer.packet_time_frames = parameters.packet_time_frames;
    writer.rtp_packet.payload_type(parameters.payload_type);
    writer.rtp_packet.ssrc(ssrc);
    writer.outgoing_data.resize(rav::rtp::AudioSender::k_buffer_num_packets);
    writer.intermediate_audio_buffer.resize(rav::rtp::AudioSender::k_max_num_frames * audio_format.bytes_per_frame());
    writer.rtp_packet_buffer = rav::ByteBuffer(packet_size_bytes);
    writer.intermediate_send_buffer.resize(packet_size_bytes);
    writer.rtp_buffer.resize(rav::rtp::AudioSender::k_max_num_frames, audio_format.bytes_per_frame());
    writer.rtp_buffer.set_ground_value(audio_format.ground_value());
    writer.destinations = parameters.destinations;
    writer.id = id;

    return true;
}

void reset_writer(rav::rtp::AudioSender::Writer& writer) {
    // Not clearing rw_lock to maintain lock
    writer.id = {};
    writer.destinations = {};
    writer.rtp_packet_buffer = {};
    writer.intermediate_send_buffer = {};
    writer.intermediate_audio_buffer = {};
    writer.packet_time_frames = {};
    writer.rtp_packet = {};
    writer.audio_format = {};
    writer.rtp_buffer = rav::rtp::Ringbuffer {};
    writer.outgoing_data.reset();

    for (auto& socket : writer.sockets) {
        if (socket.is_open()) {
            boost::system::error_code ec;
            socket.close(ec);
            if (ec) {
                RAV_LOG_ERROR("Failed to close socket: {}", ec.message());
            }
        }
    }
}

boost::system::error_code set_error(rav::rtp::AudioSender& sender, const boost::system::error_code& ec) {
    if (ec == sender.last_error) {
        return {};
    }
    sender.last_error = ec;
    return sender.last_error;
}

bool schedule_data_for_sending_realtime(
    rav::rtp::AudioSender::Writer& writer, const rav::BufferView<const uint8_t> buffer, const uint32_t timestamp
) {
    auto& rtp_buffer = writer.rtp_buffer;
    auto& rtp_packet = writer.rtp_packet;
    const auto packet_time_frames = writer.packet_time_frames;
    const auto size_per_packet = packet_time_frames * writer.audio_format.bytes_per_frame();

    if (rtp_buffer.get_next_ts() != rav::WrappingUint32(timestamp)) {
        // This buffer is not at the expected timestamp, reset the timestamp
        rtp_packet.set_timestamp(timestamp);
        rtp_buffer.set_next_ts(timestamp);
    }

    rtp_buffer.clear_until(timestamp);
    rtp_buffer.write(timestamp, buffer);

    const auto next_ts = rtp_buffer.get_next_ts();

    while (rtp_packet.get_timestamp() + packet_time_frames < next_ts) {
        TRACY_ZONE_SCOPED;

        rtp_buffer.read(rtp_packet.get_timestamp().value(), writer.intermediate_send_buffer.data(), size_per_packet);

        writer.rtp_packet_buffer.clear();
        rtp_packet.encode(writer.intermediate_send_buffer.data(), size_per_packet, writer.rtp_packet_buffer);

        RAV_ASSERT_DEBUG(writer.rtp_packet_buffer.size() <= rav::aes67::constants::k_max_payload, "Packet payload overflow");

        if (writer.rtp_packet_buffer.size() > rav::aes67::constants::k_max_payload) {
            return false;
        }

        rav::rtp::AudioSender::FifoPacket packet;
        packet.rtp_timestamp = rtp_packet.get_timestamp().value();
        packet.payload_size_bytes = static_cast<uint32_t>(writer.rtp_packet_buffer.size());
        std::memcpy(packet.payload.data(), writer.rtp_packet_buffer.data(), writer.rtp_packet_buffer.size());

        if (!writer.outgoing_data.push(packet)) {
            writer.num_packets_failed_to_schedule.fetch_add(1, std::memory_order_relaxed);
        }

        rtp_packet.sequence_number_inc(1);
        rtp_packet.inc_timestamp(packet_time_frames);
    }

    return true;
}

}  // namespace

rav::rtp::AudioSender::AudioSender(boost::asio::io_context& io_context) {
    for (size_t i = writers.size(); i < decltype(writers)::capacity(); i++) {
        writers.emplace_back(generate_array<udp_socket, k_max_num_redundant_sessions>([&io_context](std::size_t) {
            return udp_socket(io_context);
        }));
    }
}

rav::rtp::AudioSender::~AudioSender() {
    for (auto& writer : writers) {
        RAV_ASSERT_NO_THROW(!writer.id.is_valid(), "There should be no active writers at this point");
    }
}

bool rav::rtp::AudioSender::add_writer(const Id id, const WriterParameters& parameters, const ArrayOfAddresses& interfaces) {
    for (auto& writer : writers) {
        if (writer.id == id) {
            RAV_LOG_WARNING("A writer for given id already exists");
            return false;
        }
    }

    for (auto& writer : writers) {
        const auto guard = writer.rw_lock.lock_exclusive();
        if (!guard) {
            RAV_LOG_ERROR("Failed to exclusively lock writer");
            return false;
        }

        if (writer.id.is_valid()) {
            continue;  // In use already
        }

        RAV_LOG_TRACE("Adding writer {}", id.value());
        return setup_writer(writer, id, parameters, interfaces);
    }

    return true;
}

bool rav::rtp::AudioSender::remove_writer(const Id id) {
    for (auto& writer : writers) {
        if (writer.id == id) {
            const auto guard = writer.rw_lock.lock_exclusive();
            if (!guard) {
                RAV_LOG_ERROR("Failed to exclusively lock writer");
                return false;
            }
            RAV_LOG_TRACE("Removing writer {}", writer.id.value());
            reset_writer(writer);
            return true;
        }
    }

    return false;
}

bool rav::rtp::AudioSender::set_interfaces(const ArrayOfAddresses& interfaces) {
    for (auto& writer : writers) {
        const auto guard = writer.rw_lock.lock_exclusive();
        if (!guard) {
            RAV_LOG_ERROR("Failed to exclusively lock writer");
            return false;
        }

        RAV_ASSERT(interfaces.size() == writer.sockets.size(), "Sockets and interfaces should be equal");

        if (!writer.id.is_valid()) {
            continue;  // Writer not in use
        }

        for (size_t i = 0; i < interfaces.size(); i++) {
            boost::system::error_code ec;
            writer.sockets[i].set_option(boost::asio::ip::multicast::outbound_interface(interfaces[i]), ec);
            if (ec) {
                RAV_LOG_ERROR("Failed to set interface: {}", ec.message());
            }
        }
    }

    return true;
}

bool rav::rtp::AudioSender::set_ttl(const Id id, const uint8_t ttl) {
    for (auto& writer : writers) {
        if (writer.id == id) {
            const auto guard = writer.rw_lock.lock_exclusive();
            if (!guard) {
                RAV_LOG_ERROR("Failed to exclusively lock writer");
                return false;
            }

            for (auto& socket : writer.sockets) {
                if (!set_socket_ttl(socket, ttl)) {
                    return false;
                }
            }
            return true;
        }
    }

    return false;
}

void rav::rtp::AudioSender::send_outgoing_packets() {
    TRACY_ZONE_SCOPED;

    for (auto& writer : writers) {
        const auto guard = writer.rw_lock.try_lock_shared();
        if (!guard) {
            continue;  // Exclusive locked, so it either just appeared or is about to go away.
        }

        for (size_t i = 0; i < writer.outgoing_data.size(); ++i) {
            const auto packet = writer.outgoing_data.pop();

            if (!packet.has_value()) {
                return;  // Nothing to do here
            }

            RAV_ASSERT_DEBUG(packet->payload_size_bytes <= aes67::constants::k_max_payload, "Payload size exceeds maximum");
            RAV_ASSERT_DEBUG(packet->payload_size_bytes > 0, "Packet is empty");

            for (size_t j = 0; j < writer.destinations.size(); j++) {
                if (writer.destinations[j].address().is_unspecified()) {
                    continue;
                }
                if (writer.destinations[j].port() == 0) {
                    continue;
                }

                boost::system::error_code ec;
                writer.sockets[j].send_to(
                    boost::asio::buffer(packet->payload.data(), packet->payload_size_bytes), writer.destinations[j], 0, ec
                );
                if (set_error(*this, ec)) {
                    writer.num_packets_failed_to_send.fetch_add(1, std::memory_order_relaxed);
                }

                RAV_ASSERT_DEBUG(PacketView(packet->payload.data(), packet->payload_size_bytes).validate(), "Packet validation failed");
            }
        }
    }
}

bool rav::rtp::AudioSender::send_data_realtime(const Id id, const BufferView<const uint8_t> buffer, const uint32_t timestamp) {
    TRACY_ZONE_SCOPED;

    for (auto& writer : writers) {
        const auto guard = writer.rw_lock.lock_shared();
        if (!guard) {
            continue;
        }
        if (writer.id != id) {
            continue;
        }
        return schedule_data_for_sending_realtime(writer, buffer, timestamp);
    }

    return false;
}

bool rav::rtp::AudioSender::send_audio_data_realtime(
    const Id id, const AudioBufferView<const float>& input_buffer, const uint32_t timestamp
) {
    TRACY_ZONE_SCOPED;

    for (auto& writer : writers) {
        const auto guard = writer.rw_lock.lock_shared();
        if (!guard) {
            continue;
        }
        if (writer.id != id) {
            continue;
        }

        if (input_buffer.num_frames() > k_max_num_frames) {
            RAV_ASSERT_DEBUG(false, "Input buffer size exceeds maximum");
            return false;
        }

        const auto audio_format = writer.audio_format;
        if (audio_format.num_channels != input_buffer.num_channels()) {
            RAV_ASSERT_DEBUG(false, "Channel mismatch");
            return false;
        }

        auto& intermediate_buffer = writer.intermediate_audio_buffer;

        if (audio_format.encoding == AudioEncoding::pcm_s16) {
            AudioData::convert<float, AudioData::ByteOrder::Ne, int16_t, AudioData::ByteOrder::Be, AudioData::Interleaving::Interleaved>(
                input_buffer.data(), input_buffer.num_frames(), input_buffer.num_channels(),
                reinterpret_cast<int16_t*>(intermediate_buffer.data()), 0, 0
            );
        } else if (audio_format.encoding == AudioEncoding::pcm_s24) {
            AudioData::convert<float, AudioData::ByteOrder::Ne, int24_t, AudioData::ByteOrder::Be, AudioData::Interleaving::Interleaved>(
                input_buffer.data(), input_buffer.num_frames(), input_buffer.num_channels(),
                reinterpret_cast<int24_t*>(intermediate_buffer.data()), 0, 0
            );
        }

        return schedule_data_for_sending_realtime(
            writer, BufferView(intermediate_buffer).subview(0, input_buffer.num_frames() * audio_format.bytes_per_frame()).const_view(),
            timestamp
        );
    }

    return false;
}
