/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/rtp_audio_receiver.hpp"

#include "ravennakit/core/exclusive_access_guard.hpp"
#include "ravennakit/core/audio/audio_data.hpp"
#include "ravennakit/core/chrono/high_resolution_clock.hpp"
#include "ravennakit/core/types/int24.hpp"

rav::rtp::AudioReceiver::AudioReceiver(asio::io_context& io_context, Receiver& rtp_receiver) :
    rtp_receiver_(rtp_receiver), maintenance_timer_(io_context) {}

rav::rtp::AudioReceiver::~AudioReceiver() {
    maintenance_timer_.cancel();  // TODO: I don't think this is safe. The completion token might outlive this object.
                                  // The timer has a low interval which might the reason that we haven't run into this.
    rtp_receiver_.unsubscribe(this);
}

bool rav::rtp::AudioReceiver::set_parameters(Parameters new_parameters) {
    if (new_parameters == parameters_) {
        return false;  // No change in parameters
    }

    parameters_ = std::move(new_parameters);

    // Defer destruction of stream contexts until the shared context is updated
    const auto* data = stream_contexts_.data();
    const auto deferred = std::move(stream_contexts_);
    RAV_ASSERT(deferred.data() == data, "Expecting stream contexts to be moved");
    RAV_ASSERT(stream_contexts_.empty(), "Expecting stream contexts to be empty");

    for (const auto& stream : parameters_.streams) {
        stream_contexts_.emplace_back(std::make_unique<StreamContext>(stream));
    }

    stop();
    update_shared_context();
    start();

    return true;
}

const rav::rtp::AudioReceiver::Parameters& rav::rtp::AudioReceiver::get_parameters() const {
    return parameters_;
}

std::optional<uint32_t> rav::rtp::AudioReceiver::read_data_realtime(
    uint8_t* buffer, const size_t buffer_size, const std::optional<uint32_t> at_timestamp
) {
    TRACY_ZONE_SCOPED;

    if (auto lock = audio_thread_reader_.lock_realtime()) {
        RAV_ASSERT_EXCLUSIVE_ACCESS(realtime_access_guard_);
        RAV_ASSERT(buffer_size != 0, "Buffer size must be greater than 0");
        RAV_ASSERT(buffer != nullptr, "Buffer must not be nullptr");

        do_realtime_maintenance();

        if (buffer_size > lock->read_buffer.size()) {
            RAV_WARNING("Buffer size is larger than the read buffer size");
            return std::nullopt;
        }

        if (at_timestamp.has_value()) {
            lock->next_ts = *at_timestamp;
        }

        const auto num_frames = static_cast<uint32_t>(buffer_size) / lock->selected_audio_format.bytes_per_frame();

        const auto read_at = lock->next_ts.value();
        if (!lock->receiver_buffer.read(read_at, buffer, buffer_size, true)) {
            return std::nullopt;
        }

        lock->next_ts += num_frames;

        return read_at;
    }

    return std::nullopt;
}

std::optional<uint32_t> rav::rtp::AudioReceiver::read_audio_data_realtime(
    AudioBufferView<float> output_buffer, const std::optional<uint32_t> at_timestamp
) {
    TRACY_ZONE_SCOPED;

    RAV_ASSERT(output_buffer.is_valid(), "Buffer must be valid");

    if (auto lock = audio_thread_reader_.lock_realtime()) {
        const auto format = lock->selected_audio_format;

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

        auto& buffer = lock->read_buffer;
        const auto read_at =
            read_data_realtime(buffer.data(), output_buffer.num_frames() * format.bytes_per_frame(), at_timestamp);

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

rav::rtp::AudioReceiver::SessionStats rav::rtp::AudioReceiver::get_session_stats(const Rank rank) const {
    SessionStats s;
    for (auto& session_context : stream_contexts_) {
        if (session_context->stream_info.rank == rank) {
            s.packet_stats = session_context->packet_stats.get_total_counts();
            s.packet_interval_stats = session_context->packet_interval_stats.get_stats();
            return s;
        }
    }
    return s;
}

std::optional<rav::rtp::AudioReceiver::State>
rav::rtp::AudioReceiver::get_state_for_stream(const Session& session) const {
    if (const auto* session_context = find_stream_context(session)) {
        return session_context->state;
    }
    return {};
}

void rav::rtp::AudioReceiver::set_delay_frames(const uint32_t delay_frames) {
    if (delay_frames_ == delay_frames) {
        return;
    }
    delay_frames_ = delay_frames;
    update_shared_context();
}

void rav::rtp::AudioReceiver::set_enabled(const bool enabled) {
    if (enabled_ == enabled) {
        return;
    }
    enabled_ = enabled;
    enabled_ ? start() : stop();
}

void rav::rtp::AudioReceiver::set_interfaces(const std::map<Rank, asio::ip::address_v4>& interface_addresses) {
    if (interface_addresses_ == interface_addresses) {
        return; // No change in interface addresses
    }
    interface_addresses_ = interface_addresses;
    stop();
    start();
}

void rav::rtp::AudioReceiver::on_data_received(std::function<void(WrappingUint32 packet_timestamp)> callback) {
    on_data_received_callback_ = std::move(callback);
}

void rav::rtp::AudioReceiver::on_data_ready(std::function<void(WrappingUint32 packet_timestamp)> callback) {
    on_data_ready_callback_ = std::move(callback);
}

void rav::rtp::AudioReceiver::on_state_changed(std::function<void(const Stream& stream, State state)> callback) {
    on_state_changed_callback_ = std::move(callback);
}

const char* rav::rtp::AudioReceiver::to_string(const State state) {
    switch (state) {
        case State::idle:
            return "idle";
        case State::waiting_for_data:
            return "waiting_for_data";
        case State::ok:
            return "ok";
        case State::ok_no_consumer:
            return "ok_no_consumer";
        case State::inactive:
            return "inactive";
        default:
            return "unknown";
    }
}

rav::rtp::AudioReceiver::StreamContext::StreamContext(Stream info) :
    stream_info(std::move(info)), last_packet_time_ns(HighResolutionClock::now()) {}

void rav::rtp::AudioReceiver::on_rtp_packet(const Receiver::RtpPacketEvent& rtp_event) {
    TRACY_ZONE_SCOPED;

    auto* stream_context = find_stream_context(rtp_event.session);
    if (!stream_context) {
        RAV_ERROR("No session context found for session {}", rtp_event.session.to_string());
        return;
    }

    if (!stream_context->stream_info.filter.is_valid_source(
            rtp_event.dst_endpoint.address(), rtp_event.src_endpoint.address()
        )) {
        return;  // This packet is not for us
    }

    const WrappingUint32 packet_timestamp(rtp_event.packet.timestamp());

    if (!rtp_ts_.has_value()) {
        seq_ = rtp_event.packet.sequence_number();
        rtp_ts_ = rtp_event.packet.timestamp();
        stream_context->last_packet_time_ns = rtp_event.recv_time;
    }

    const auto payload = rtp_event.packet.payload_data();
    if (payload.size_bytes() == 0) {
        RAV_WARNING("Received packet with empty payload");
        return;
    }

    if (payload.size_bytes() > std::numeric_limits<uint16_t>::max()) {
        RAV_WARNING("Payload size exceeds maximum size");
        return;
    }

    if (const auto interval = stream_context->last_packet_time_ns.update(rtp_event.recv_time)) {
        TRACY_PLOT("packet interval (ms)", static_cast<double>(*interval) / 1'000'000.0);
        stream_context->packet_interval_stats.add(static_cast<double>(*interval) / 1'000'000.0);
    }

    if (packet_interval_throttle_.update()) {
        RAV_TRACE("Packet interval stats: {}", stream_context->packet_interval_stats.to_string());
    }

    if (auto lock = network_thread_reader_.lock_realtime()) {
        if (consumer_active_) {
            IntermediatePacket intermediate {};
            intermediate.timestamp = rtp_event.packet.timestamp();
            intermediate.seq = rtp_event.packet.sequence_number();
            intermediate.data_len = static_cast<uint16_t>(payload.size_bytes());
            intermediate.packet_time_frames = stream_context->stream_info.packet_time_frames;
            std::memcpy(intermediate.data.data(), payload.data(), intermediate.data_len);

            if (stream_context->fifo.push(intermediate)) {
                set_state(*stream_context, State::ok);
            } else {
                RAV_TRACE("Failed to push packet info FIFO, make receiver inactive");
                consumer_active_.store(false);
                set_state(*stream_context, State::ok_no_consumer);
            }
        } else {
            set_state(*stream_context, State::ok_no_consumer);
        }

        while (auto seq = stream_context->packets_too_old.pop()) {
            stream_context->packet_stats.mark_packet_too_late(*seq);
        }

        if (const auto stats = stream_context->packet_stats.update(rtp_event.packet.sequence_number())) {
            if (auto v = packet_stats_throttle_.update(*stats)) {
                RAV_WARNING("Stats for stream {}: {}", stream_context->stream_info.session.to_string(), v->to_string());
            }
        }

        if (const auto diff = seq_.update(rtp_event.packet.sequence_number())) {
            if (diff >= 1) {
                // Only call back with monotonically increasing sequence numbers
                if (on_data_received_callback_) {
                    on_data_received_callback_(packet_timestamp);
                }
            }

            if (packet_timestamp - lock->delay_frames >= *rtp_ts_) {
                // Make sure to inserts calls for missing packets
                for (uint16_t i = 0; i < *diff; ++i) {
                    if (on_data_ready_callback_) {
                        on_data_ready_callback_(
                            packet_timestamp - lock->delay_frames
                            - (*diff - 1u - i) * stream_context->stream_info.packet_time_frames
                        );
                    }
                }
            }
        }
    }
}

void rav::rtp::AudioReceiver::on_rtcp_packet(const Receiver::RtcpPacketEvent& rtcp_event) {
    RAV_TRACE(
        "{} for session {} from {}:{}", rtcp_event.packet.to_string(), rtcp_event.session.to_string(),
        rtcp_event.src_endpoint.address().to_string(), rtcp_event.src_endpoint.port()
    );
}

void rav::rtp::AudioReceiver::update_shared_context() {
    if (enabled_ == false) {
        shared_context_.clear();
        return;
    }

    if (parameters_.streams.empty()) {
        RAV_ERROR("No streams available - clearing shared context");
        shared_context_.clear();
        return;
    }

    if (!parameters_.audio_format.is_valid()) {
        RAV_ERROR("Invalid audio format - clearing shared context");
        shared_context_.clear();
        return;
    }

    // Find the smallest packet time frames
    uint16_t packet_time_frames = parameters_.streams.front().packet_time_frames;
    for (const auto& stream : parameters_.streams) {
        if (stream.packet_time_frames < packet_time_frames) {
            packet_time_frames = stream.packet_time_frames;
        }
    }

    const auto bytes_per_frame = parameters_.audio_format.bytes_per_frame();
    RAV_ASSERT(bytes_per_frame > 0, "bytes_per_frame must be greater than 0");

    auto new_context = std::make_unique<SharedContext>();

    const auto buffer_size_frames = std::max(parameters_.audio_format.sample_rate * k_buffer_size_ms / 1000, 1024u);
    new_context->receiver_buffer.resize(
        parameters_.audio_format.sample_rate * k_buffer_size_ms / 1000, bytes_per_frame
    );
    new_context->read_buffer.resize(buffer_size_frames * bytes_per_frame);
    const auto buffer_size_packets = buffer_size_frames / packet_time_frames;
    new_context->selected_audio_format = parameters_.audio_format;
    new_context->delay_frames = delay_frames_;

    for (auto& stream_context : stream_contexts_) {
        stream_context->fifo.resize(buffer_size_packets);
        stream_context->packets_too_old.resize(buffer_size_packets);
        new_context->stream_contexts.push_back(stream_context.get());
    }

    shared_context_.update_reclaim_all(std::move(new_context));

    do_maintenance();
}

void rav::rtp::AudioReceiver::do_maintenance() {
    // Check if streams became are no longer receiving data
    // TODO: Make this global for all streams or stream specific
    const auto now = HighResolutionClock::now();

    for (auto& stream_context : stream_contexts_) {
        if (stream_context->state == State::ok || stream_context->state == State::ok_no_consumer) {
            if ((stream_context->last_packet_time_ns + k_receive_timeout_ms * 1'000'000).value() < now) {
                set_state(*stream_context, State::inactive);
            }
        }
    }
    std::ignore = shared_context_.reclaim();

    maintenance_timer_.expires_after(std::chrono::seconds(1));
    maintenance_timer_.async_wait([this](const asio::error_code ec) {
        if (ec) {
            if (ec == asio::error::operation_aborted) {
                return;
            }
            RAV_ERROR("Timer error: {}", ec.message());
            return;
        }
        do_maintenance();
    });
}

void rav::rtp::AudioReceiver::do_realtime_maintenance() {
    if (auto lock = audio_thread_reader_.lock_realtime()) {
        const auto clear_fifo = consumer_active_.exchange(true) == false;

        for (auto* stream_context : lock->stream_contexts) {
            if (clear_fifo) {
                stream_context->fifo.pop_all();
                continue;
            }
            while (const auto packet = stream_context->fifo.pop()) {
                WrappingUint32 packet_timestamp(packet->timestamp);
                if (!lock->first_packet_timestamp) {
                    RAV_TRACE("First packet timestamp: {}", packet->timestamp);
                    lock->first_packet_timestamp = packet_timestamp;
                    lock->receiver_buffer.set_next_ts(packet->timestamp);
                    lock->next_ts = packet_timestamp - lock->delay_frames;
                }

                // Determine whether whole packet is too old
                if (packet_timestamp + packet->packet_time_frames <= lock->next_ts) {
                    // RAV_WARNING("Packet too late: seq={}, ts={}", packet->seq, packet->timestamp);
                    TRACY_MESSAGE("Packet too late - skipping");
                    if (!stream_context->packets_too_old.push(packet->seq)) {
                        RAV_ERROR("Packet not enqueued to packets_too_old");
                    }
                    continue;
                }

                // Determine whether packet contains outdated data
                if (packet_timestamp < lock->next_ts) {
                    RAV_WARNING("Packet partly too late: seq={}, ts={}", packet->seq, packet->timestamp);
                    TRACY_MESSAGE("Packet partly too late - not skipping");
                    if (!stream_context->packets_too_old.push(packet->seq)) {
                        RAV_ERROR("Packet not enqueued to packets_too_old");
                    }
                    // Still process the packet since it contains data that is not outdated
                }

                lock->receiver_buffer.clear_until(packet->timestamp);

                if (!lock->receiver_buffer.write(packet->timestamp, {packet->data.data(), packet->data_len})) {
                    RAV_ERROR("Packet not written to buffer");
                }
            }
        }

        TRACY_PLOT("available_frames", static_cast<int64_t>(lock->next_ts.diff(lock->receiver_buffer.get_next_ts())));
    }
}

void rav::rtp::AudioReceiver::set_state(StreamContext& stream_context, const State new_state) const {
    if (stream_context.state == new_state) {
        return;
    }
    stream_context.state = new_state;
    RAV_TRACE("Session {} changed state to: {}", stream_context.stream_info.session.to_string(), to_string(new_state));
    if (on_state_changed_callback_) {
        on_state_changed_callback_(stream_context.stream_info, new_state);
    }
}

void rav::rtp::AudioReceiver::start() {
    if (is_running_) {
        RAV_ASSERT(enabled_, "Receiver is running while not enabled");
        return;
    }
    if (!enabled_) {
        return;
    }
    rtp_ts_.reset();

    for (const auto& stream : stream_contexts_) {
        if (!stream->stream_info.session.valid()) {
            continue;
        }
        auto iface = interface_addresses_.find(stream->stream_info.rank);
        if (iface == interface_addresses_.end() || iface->second.is_unspecified()) {
            continue;  // No interface address available for this stream
        }
        RAV_ASSERT(!iface->second.is_unspecified(), "Interface address must not be unspecified");
        RAV_ASSERT(!iface->second.is_multicast(), "Interface address must not be multicast");
        // Multiple streams might have the same session, but subscribing more than once for the same session has no
        // effect, so no need to do clever stuff here.
        std::ignore = rtp_receiver_.subscribe(this, stream->stream_info.session, iface->second);
    }

    is_running_ = true;
}

void rav::rtp::AudioReceiver::stop() {
    if (!is_running_) {
        return;
    }
    is_running_ = false;
    rtp_receiver_.unsubscribe(this);
}

rav::rtp::AudioReceiver::StreamContext* rav::rtp::AudioReceiver::find_stream_context(const Session& session) {
    for (const auto& context : stream_contexts_) {
        if (context->stream_info.session == session) {
            return context.get();
        }
    }
    return nullptr;
}

const rav::rtp::AudioReceiver::StreamContext*
rav::rtp::AudioReceiver::find_stream_context(const Session& session) const {
    for (auto& context : stream_contexts_) {
        if (context->stream_info.session == session) {
            return context.get();
        }
    }
    return nullptr;
}
