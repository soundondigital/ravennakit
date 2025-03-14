/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/rtp_stream_receiver.hpp"

#include "ravennakit/aes67/aes67_packet_time.hpp"
#include "ravennakit/core/tracy.hpp"
#include "ravennakit/core/audio/audio_data.hpp"
#include "ravennakit/core/chrono/high_resolution_clock.hpp"
#include "ravennakit/core/util/todo.hpp"

namespace {

bool is_connection_info_valid(const rav::sdp::connection_info_field& conn) {
    if (conn.network_type != rav::sdp::netw_type::internet) {
        RAV_WARNING("Unsupported network type in connection_info_field");
        return false;
    }
    if (!(conn.address_type == rav::sdp::addr_type::ipv4 || conn.address_type == rav::sdp::addr_type::ipv6)) {
        RAV_WARNING("Unsupported network type in connection_info_field");
        return false;
    }
    if (auto& num_addrs = conn.number_of_addresses) {
        if (num_addrs > 1) {
            RAV_WARNING("Unsupported number of addresses in connection_info_field");
            return false;
        }
    }
    return true;
}

}  // namespace

const char* rav::rtp_stream_receiver::to_string(const receiver_state state) {
    switch (state) {
        case receiver_state::idle:
            return "idle";
        case receiver_state::waiting_for_data:
            return "waiting_for_data";
        case receiver_state::ok:
            return "ok";
        case receiver_state::ok_no_consumer:
            return "ok_no_consumer";
        case receiver_state::inactive:
            return "inactive";
        default:
            return "unknown";
    }
}

std::string rav::rtp_stream_receiver::stream_updated_event::to_string() const {
    return fmt::format(
        "id={}, session={}, selected_audio_format={}, packet_time_frames={}, delay_samples={}, state={}",
        receiver_id.value(), session.to_string(), selected_audio_format.to_string(), packet_time_frames, delay_samples,
        rtp_stream_receiver::to_string(state)
    );
}

rav::rtp_stream_receiver::rtp_stream_receiver(rtp_receiver& receiver) :
    rtp_receiver_(receiver), maintenance_timer_(receiver.get_io_context()) {}

rav::rtp_stream_receiver::~rtp_stream_receiver() {
    if (!rtp_receiver_.unsubscribe(this)) {
        RAV_ERROR("Failed to remove subscriber");
    }
    maintenance_timer_.cancel();
}

rav::id rav::rtp_stream_receiver::get_id() const {
    return id_;
}

void rav::rtp_stream_receiver::update_sdp(const sdp::session_description& sdp) {
    const sdp::media_description* selected_media_description = nullptr;
    const sdp::connection_info_field* selected_connection_info = nullptr;
    std::optional<audio_format> selected_audio_format;

    for (auto& media_description : sdp.media_descriptions()) {
        if (media_description.media_type() != "audio") {
            // TODO: Query subclass for supported media types
            RAV_WARNING("Unsupported media type: {}", media_description.media_type());
            continue;
        }

        if (media_description.protocol() != "RTP/AVP") {
            // TODO: Query subclass for supported protocols
            RAV_WARNING("Unsupported protocol {}", media_description.protocol());
            continue;
        }

        // The first acceptable payload format from the beginning of the list SHOULD be used for the session.
        // https://datatracker.ietf.org/doc/html/rfc8866#name-media-descriptions-m
        // TODO: Query subclass for supported formats (by looping the available formats)
        selected_audio_format.reset();  // Reset format from previous iteration
        for (auto& format : media_description.formats()) {
            selected_audio_format = format.to_audio_format();
            if (!selected_audio_format) {
                RAV_WARNING("Not a supported audio format: {}", format.to_string());
                continue;
            }
            break;
        }

        if (!selected_audio_format) {
            RAV_WARNING("No supported audio format found");
            continue;
        }

        for (auto& conn : media_description.connection_infos()) {
            if (!is_connection_info_valid(conn)) {
                continue;
            }
            selected_connection_info = &conn;
        }

        selected_media_description = &media_description;
        break;
    }

    if (!selected_media_description) {
        RAV_WARNING("No suitable media description found in SDP");
        return;
    }

    if (!selected_audio_format) {
        RAV_WARNING("No media description with supported audio format available");
        return;
    }

    if (selected_connection_info == nullptr) {
        if (const auto& conn = sdp.connection_info()) {
            if (is_connection_info_valid(*conn)) {
                selected_connection_info = &*conn;
            }
        }
    }

    if (selected_connection_info == nullptr) {
        RAV_WARNING("No suitable connection info found in SDP");
        return;
    }

    RAV_ASSERT(selected_connection_info != nullptr, "Expecting found_connection_info to be set");

    uint16_t packet_time_frames = 0;
    const auto ptime = selected_media_description->ptime();
    if (ptime.has_value()) {
        packet_time_frames = aes67_packet_time::framecount(*ptime, selected_audio_format->sample_rate);
    }

    if (packet_time_frames == 0) {
        RAV_WARNING("No ptime attribute found, falling back to framecount");
        const auto framecount = selected_media_description->framecount();
        if (!framecount.has_value()) {
            RAV_ERROR("No framecount attribute found");
            return;
        }
        packet_time_frames = *framecount;
    }

    RAV_ASSERT(packet_time_frames > 0, "packet_time_frames must be greater than 0");

    rtp_session session;
    session.connection_address = asio::ip::make_address(selected_connection_info->address);  // TODO: Avoid exception
    session.rtp_port = selected_media_description->port();
    session.rtcp_port = session.rtp_port + 1;

    rtp_filter filter(session.connection_address);

    const auto& source_filters = selected_media_description->source_filters();
    if (!source_filters.empty()) {
        if (filter.add_filters(source_filters) == 0) {
            RAV_WARNING("No suitable source filters found in SDP");
        }
    } else {
        const auto& sdp_source_filters = sdp.source_filters();
        if (!sdp_source_filters.empty()) {
            if (filter.add_filters(sdp_source_filters) == 0) {
                RAV_WARNING("No suitable source filters found in SDP");
            }
        }
    }

    bool should_restart = false;  // Implies was_changed
    auto was_changed = false;
    auto [stream, was_created] = find_or_create_media_stream(session);

    RAV_ASSERT(stream != nullptr, "Stream must not be nullptr");

    // Session
    if (stream->session != session || was_created) {
        should_restart = true;
        stream->session = session;
    }

    // Filter
    if (stream->filter != filter) {
        was_changed = true;
        stream->filter = filter;
    }

    // Packet time
    if (stream->packet_time_frames != packet_time_frames) {
        was_changed = true;
        stream->packet_time_frames = packet_time_frames;
    }

    // Audio format
    if (stream->selected_format != *selected_audio_format) {
        should_restart = true;
        RAV_TRACE(
            "Audio format changed from {} to {}", stream->selected_format.to_string(),
            selected_audio_format->to_string()
        );
        stream->selected_format = *selected_audio_format;
    }

    // Delete all streams that are not in the SDP anymore
    for (auto it = media_streams_.begin(); it != media_streams_.end();) {
        if (it->session != session) {
            it = media_streams_.erase(it);
            should_restart = true;
        } else {
            ++it;
        }
    }

    if (should_restart) {
        restart();
        set_state(receiver_state::waiting_for_data, false);
    }

    if (should_restart || was_changed) {
        auto event = make_updated_event();
        for (auto& s : subscribers_) {
            s->rtp_stream_receiver_updated(event);
        }
    }
}

void rav::rtp_stream_receiver::set_delay(const uint32_t delay) {
    if (delay == delay_) {
        return;
    }
    delay_ = delay;

    // TODO: Update realtime_context_.next_ts with the difference between the old and new delay. Or call restart()?

    const auto event = make_updated_event();
    for (auto* s : subscribers_) {
        s->rtp_stream_receiver_updated(event);
    }
}

uint32_t rav::rtp_stream_receiver::get_delay() const {
    return delay_;
}

bool rav::rtp_stream_receiver::subscribe(subscriber* subscriber) {
    if (subscribers_.add(subscriber)) {
        subscriber->rtp_stream_receiver_updated(make_updated_event());
        return true;
    }
    return false;
}

bool rav::rtp_stream_receiver::unsubscribe(subscriber* subscriber) {
    return subscribers_.remove(subscriber);
}

std::optional<uint32_t> rav::rtp_stream_receiver::read_data_realtime(
    uint8_t* buffer, const size_t buffer_size, const std::optional<uint32_t> at_timestamp
) {
    TRACY_ZONE_SCOPED;

    if (auto state = audio_thread_reader_.lock_realtime()) {
        RAV_ASSERT_EXCLUSIVE_ACCESS(realtime_access_guard_);
        RAV_ASSERT(buffer_size != 0, "Buffer size must be greater than 0");
        RAV_ASSERT(buffer != nullptr, "Buffer must not be nullptr");

        do_realtime_maintenance();

        if (buffer_size > state->read_buffer.size()) {
            RAV_WARNING("Buffer size is larger than the read buffer size");
            return std::nullopt;
        }

        if (!state->first_packet_timestamp.has_value()) {
            return std::nullopt;
        }

        if (at_timestamp.has_value()) {
            state->next_ts = *at_timestamp;
        }

        const auto num_frames = static_cast<uint32_t>(buffer_size) / state->selected_audio_format.bytes_per_frame();

        const auto read_at = state->next_ts.value();
        if (!state->receiver_buffer.read(read_at, buffer, buffer_size)) {
            return std::nullopt;
        }

        state->next_ts += num_frames;

        return read_at;
    }

    return std::nullopt;
}

std::optional<uint32_t> rav::rtp_stream_receiver::read_audio_data_realtime(
    audio_buffer_view<float> output_buffer, const std::optional<uint32_t> at_timestamp
) {
    TRACY_ZONE_SCOPED;

    RAV_ASSERT(output_buffer.is_valid(), "Buffer must be valid");

    if (auto state = audio_thread_reader_.lock_realtime()) {
        const auto format = state->selected_audio_format;

        if (format.byte_order != audio_format::byte_order::be) {
            RAV_ERROR("Unexpected byte order");
            return std::nullopt;
        }

        if (format.ordering != audio_format::channel_ordering::interleaved) {
            RAV_ERROR("Unexpected channel ordering");
            return std::nullopt;
        }

        if (format.num_channels != output_buffer.num_channels()) {
            RAV_ERROR("Channel mismatch");
            return std::nullopt;
        }

        auto& buffer = state->read_buffer;
        const auto read_at =
            read_data_realtime(buffer.data(), output_buffer.num_frames() * format.bytes_per_frame(), at_timestamp);

        if (!read_at.has_value()) {
            return std::nullopt;
        }

        if (format.encoding == audio_encoding::pcm_s16) {
            const auto ok = audio_data::convert<
                int16_t, audio_data::byte_order::be, audio_data::interleaving::interleaved, float,
                audio_data::byte_order::ne>(
                reinterpret_cast<int16_t*>(buffer.data()), output_buffer.num_frames(), output_buffer.num_channels(),
                output_buffer.data()
            );
            if (!ok) {
                RAV_WARNING("Failed to convert audio data");
            }
        } else if (format.encoding == audio_encoding::pcm_s24) {
            const auto ok = audio_data::convert<
                int24_t, audio_data::byte_order::be, audio_data::interleaving::interleaved, float,
                audio_data::byte_order::ne>(
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

rav::rtp_stream_receiver::stream_stats rav::rtp_stream_receiver::get_session_stats() const {
    if (media_streams_.empty()) {
        return {};
    }
    stream_stats s;
    const auto& stream = media_streams_.front();
    s.packet_stats = stream.packet_stats.get_total_counts();
    s.packet_interval_stats = stream.packet_interval_stats.get_stats();
    return s;
}

rav::rtp_packet_stats::counters rav::rtp_stream_receiver::get_packet_stats() const {
    if (media_streams_.empty()) {
        return {};
    }
    return media_streams_.front().packet_stats.get_total_counts();
}

rav::sliding_stats::stats rav::rtp_stream_receiver::get_packet_interval_stats() const {
    if (media_streams_.empty()) {
        return {};
    }
    return media_streams_.front().packet_interval_stats.get_stats();
}

void rav::rtp_stream_receiver::restart() {
    rtp_receiver_.unsubscribe(this); // This unsubscribes from all sessions

    if (media_streams_.empty()) {
        set_state(receiver_state::idle, true);
        return;
    }

    std::optional<audio_format> selected_format;
    uint16_t packet_time_frames = 0;

    for (auto& stream : media_streams_) {
        if (!stream.selected_format.is_valid()) {
            RAV_WARNING("Invalid audio format");
            return;
        }
        if (!selected_format.has_value()) {
            selected_format = stream.selected_format;
            packet_time_frames = stream.packet_time_frames;
        } else if (stream.selected_format != *selected_format) {
            RAV_WARNING("Audio formats are not the same");
            return;
        }
    }

    if (!selected_format.has_value()) {
        RAV_WARNING("No valid audio format available");
        return;
    }

    const auto bytes_per_frame = selected_format->bytes_per_frame();
    RAV_ASSERT(bytes_per_frame > 0, "bytes_per_frame must be greater than 0");

    auto new_state = std::make_unique<shared_state>();

    const auto buffer_size_frames = std::max(selected_format->sample_rate * k_buffer_size_ms / 1000, 1024u);
    new_state->receiver_buffer.resize(selected_format->sample_rate * k_buffer_size_ms / 1000, bytes_per_frame);
    new_state->read_buffer.resize(buffer_size_frames * bytes_per_frame);
    const auto buffer_size_packets = buffer_size_frames / packet_time_frames;
    new_state->fifo.resize(buffer_size_packets);
    new_state->packets_too_old.resize(buffer_size_packets);
    new_state->selected_audio_format = *selected_format;

    shared_state_.update(std::move(new_state));

    // TODO: Synchronize with the network thread
    for (auto& stream : media_streams_) {
        stream.first_packet_timestamp.reset();
        stream.packet_stats.reset();
        if (!rtp_receiver_.subscribe(this, stream.session, stream.filter)) {
            RAV_ERROR("Failed to add subscriber");
        }
    }

    do_maintenance();

    RAV_TRACE("(Re)Started rtp_stream_receiver");
}

std::pair<rav::rtp_stream_receiver::media_stream*, bool>
rav::rtp_stream_receiver::find_or_create_media_stream(const rtp_session& session) {
    for (auto& stream : media_streams_) {
        if (stream.session == session) {
            return std::make_pair(&stream, false);
        }
    }

    return std::make_pair(&media_streams_.emplace_back(session), true);
}

void rav::rtp_stream_receiver::handle_rtp_packet_event_for_session(
    const rtp_receiver::rtp_packet_event& event, media_stream& stream
) {
    TRACY_ZONE_SCOPED;

    const wrapping_uint32 packet_timestamp(event.packet.timestamp());

    if (!stream.first_packet_timestamp.has_value()) {
        stream.seq = event.packet.sequence_number();
        stream.first_packet_timestamp = event.packet.timestamp();
        stream.last_packet_time_ns = event.recv_time;
    }

    const auto payload = event.packet.payload_data();
    if (payload.size_bytes() == 0) {
        RAV_WARNING("Received packet with empty payload");
        return;
    }

    if (payload.size_bytes() > std::numeric_limits<uint16_t>::max()) {
        RAV_WARNING("Payload size exceeds maximum size");
        return;
    }

    if (const auto interval = stream.last_packet_time_ns.update(event.recv_time)) {
        TRACY_PLOT("packet interval (ms)", static_cast<double>(*interval) / 1'000'000.0);
        stream.packet_interval_stats.add(static_cast<double>(*interval) / 1'000'000.0);
    }

    if (stream.packet_interval_throttle.update()) {
        RAV_TRACE("Packet interval stats: {}", stream.packet_interval_stats.to_string());
    }

    if (auto shared_state = network_thread_reader_.lock_realtime()) {
        if (shared_state->consumer_active) {
            intermediate_packet intermediate {};
            intermediate.timestamp = event.packet.timestamp();
            intermediate.seq = event.packet.sequence_number();
            intermediate.data_len = static_cast<uint16_t>(payload.size_bytes());
            intermediate.packet_time_frames = stream.packet_time_frames;
            std::memcpy(intermediate.data.data(), payload.data(), intermediate.data_len);

            if (!shared_state->fifo.push(intermediate)) {
                RAV_TRACE("Failed to push packet info FIFO, make receiver inactive");
                shared_state->consumer_active = false;
                set_state(receiver_state::ok_no_consumer, true);
            } else {
                set_state(receiver_state::ok, true);
            }
        } else {
            set_state(receiver_state::ok_no_consumer, true);
        }

        while (auto seq = shared_state->packets_too_old.pop()) {
            stream.packet_stats.mark_packet_too_late(*seq);
        }
    }

    if (const auto stats = stream.packet_stats.update(event.packet.sequence_number())) {
        if (auto v = stream.packet_stats_throttle.update(*stats)) {
            RAV_WARNING("Stats for stream {}: {}", stream.session.to_string(), v->to_string());
        }
    }

    if (const auto diff = stream.seq.update(event.packet.sequence_number())) {
        if (diff >= 1) {
            // Only call back with monotonically increasing sequence numbers
            for (const auto& c : subscribers_) {
                c->on_data_received(packet_timestamp);
            }
        }

        if (packet_timestamp - delay_ >= *stream.first_packet_timestamp) {
            // Make sure to call with the correct timestamps for the missing packets
            for (uint16_t i = 0; i < *diff; ++i) {
                for (const auto& c : subscribers_) {
                    c->on_data_ready(packet_timestamp - delay_ - (*diff - 1u - i) * stream.packet_time_frames);
                }
            }
        }
    }
}

void rav::rtp_stream_receiver::set_state(const receiver_state new_state, const bool notify_subscribers) {
    // TODO: Schedule this on the maintenance thread
    if (state_ == new_state) {
        return;
    }
    state_ = new_state;
    if (notify_subscribers) {
        const auto event = make_updated_event();
        for (auto* s : subscribers_) {
            s->rtp_stream_receiver_updated(event);
        }
    }
}

rav::rtp_stream_receiver::stream_updated_event rav::rtp_stream_receiver::make_updated_event() const {
    stream_updated_event event;
    event.receiver_id = id_;
    event.state = state_;
    event.delay_samples = delay_;

    if (media_streams_.empty()) {
        return event;
    }

    auto& stream = media_streams_.front();
    event.session = stream.session;
    event.filter = stream.filter;
    event.selected_audio_format = stream.selected_format;
    event.packet_time_frames = stream.packet_time_frames;
    return event;
}

void rav::rtp_stream_receiver::do_maintenance() {
    // Check if streams became are no longer receiving data
    if (state_ == receiver_state::ok || state_ == receiver_state::ok_no_consumer) {
        const auto now = high_resolution_clock::now();
        for (const auto& stream : media_streams_) {
            if ((stream.last_packet_time_ns + k_receive_timeout_ms * 1'000'000).value() < now) {
                set_state(receiver_state::inactive, true);
            }
        }
    }

    std::ignore = shared_state_.reclaim();

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

void rav::rtp_stream_receiver::do_realtime_maintenance() {
    if (auto shared_state = audio_thread_reader_.lock_realtime()) {
        if (shared_state->consumer_active.exchange(true) == false) {
            shared_state->fifo.pop_all();
        }

        while (const auto packet = shared_state->fifo.pop()) {
            wrapping_uint32 packet_timestamp(packet->timestamp);
            if (!shared_state->first_packet_timestamp) {
                RAV_TRACE("First packet timestamp: {}", packet->timestamp);
                shared_state->first_packet_timestamp = packet_timestamp;
                shared_state->receiver_buffer.set_next_ts(packet->timestamp);
                shared_state->next_ts = packet_timestamp - delay_.load();
            }

            // Determine whether whole packet is too old
            if (packet_timestamp + packet->packet_time_frames <= shared_state->next_ts) {
                // RAV_WARNING("Packet too late: seq={}, ts={}", packet->seq, packet->timestamp);
                TRACY_MESSAGE("Packet too late - skipping");
                if (!shared_state->packets_too_old.push(packet->seq)) {
                    RAV_ERROR("Packet not enqueued to packets_too_old");
                }
                continue;
            }

            // Determine whether packet contains outdated data
            if (packet_timestamp < shared_state->next_ts) {
                RAV_WARNING("Packet partly too late: seq={}, ts={}", packet->seq, packet->timestamp);
                TRACY_MESSAGE("Packet partly too late - not skipping");
                if (!shared_state->packets_too_old.push(packet->seq)) {
                    RAV_ERROR("Packet not enqueued to packets_too_old");
                }
                // Still process the packet since it contains data that is not outdated
            }

            shared_state->receiver_buffer.clear_until(packet->timestamp);

            if (!shared_state->receiver_buffer.write(packet->timestamp, {packet->data.data(), packet->data_len})) {
                RAV_ERROR("Packet not written to buffer");
            }
        }

        TRACY_PLOT(
            "available_frames",
            static_cast<int64_t>(shared_state->next_ts.diff(shared_state->receiver_buffer.next_ts()))
        );
    }
}

void rav::rtp_stream_receiver::on_rtp_packet(const rtp_receiver::rtp_packet_event& rtp_event) {
    // TODO: We should probably discard filtered packets here and not in rtp_receiver. This would also allow us to use a
    // subscriber list without context in rtp_receiver. Alternatively we could add a virtual function to
    // rtp_receiver::subscriber to determine whether the packet should be filtered or not. But since we need to call a
    // virtual function anyway (this one) we might as well filter it here.

    // RAV_TRACE(
    // "{} for session {} from {}:{}", rtp_event.packet.to_string(), rtp_event.session.to_string(),
    // rtp_event.src_endpoint.address().to_string(), rtp_event.src_endpoint.port()
    // );

    for (auto& stream : media_streams_) {
        if (rtp_event.session == stream.session) {
            handle_rtp_packet_event_for_session(rtp_event, stream);
            return;
        }
    }

    RAV_WARNING("Packet received for unknown session");
}

void rav::rtp_stream_receiver::on_rtcp_packet(const rtp_receiver::rtcp_packet_event& rtcp_event) {
    RAV_TRACE(
        "{} for session {} from {}:{}", rtcp_event.packet.to_string(), rtcp_event.session.to_string(),
        rtcp_event.src_endpoint.address().to_string(), rtcp_event.src_endpoint.port()
    );
}
