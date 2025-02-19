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

rav::rtp_stream_receiver::rtp_stream_receiver(rtp_receiver& receiver) : rtp_receiver_(receiver) {}

rav::rtp_stream_receiver::~rtp_stream_receiver() {
    rtp_receiver_.unsubscribe(*this);
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
    session.connection_address = asio::ip::make_address(selected_connection_info->address);
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

    bool should_restart = false;

    auto& stream = find_or_create_stream_info(session);

    // Session
    if (stream.session != session) {
        should_restart = true;
        stream.session = session;
    }
    stream.session = session;
    stream.filter = filter;
    stream.packet_time_frames = packet_time_frames;

    if (selected_format_ != *selected_audio_format) {
        should_restart = true;
        RAV_TRACE(
            "Audio format changed from {} to {}", selected_format_.to_string(), selected_audio_format->to_string()
        );
        for (const auto& s : subscribers_) {
            s->on_audio_format_changed(*selected_audio_format, stream.packet_time_frames);
        }
        selected_format_ = *selected_audio_format;
    }

    // Delete all streams that are not in the SDP anymore
    for (auto it = streams_.begin(); it != streams_.end();) {
        if (it->session != session) {
            it = streams_.erase(it);
            should_restart = true;
        } else {
            ++it;
        }
    }

    if (should_restart) {
        restart();
    }
}

void rav::rtp_stream_receiver::set_delay(const uint32_t delay) {
    if (delay == delay_) {
        return;
    }
    delay_ = delay;
    restart();
}

uint32_t rav::rtp_stream_receiver::get_delay() const {
    return delay_;
}

bool rav::rtp_stream_receiver::add_subscriber(subscriber* subscriber_to_add) {
    // TODO: call subscriber with current state
    if (!subscribers_.add(subscriber_to_add)) {
        return false;
    }
    return true;
}

bool rav::rtp_stream_receiver::remove_subscriber(subscriber* subscriber_to_remove) {
    return subscribers_.remove(subscriber_to_remove);
}

bool rav::rtp_stream_receiver::read_data(const uint32_t at_timestamp, uint8_t* buffer, const size_t buffer_size) {
    TRACY_ZONE_SCOPED;

    RAV_ASSERT(buffer_size != 0, "Buffer size must be greater than 0");
    RAV_ASSERT(buffer != nullptr, "Buffer must not be nullptr");

    const auto num_frames =
        static_cast<uint32_t>(buffer_size) / realtime_context_.selected_audio_format.bytes_per_frame();

    if (consumer_active_.exchange(true) == false) {
        realtime_context_.fifo.pop_all();
    }

    while (const auto packet = realtime_context_.fifo.pop()) {
        wrapping_uint32 packet_timestamp(packet->timestamp);
        if (!realtime_context_.first_packet_timestamp) {
            RAV_TRACE("First packet timestamp: {}", packet->timestamp);
            realtime_context_.first_packet_timestamp = packet_timestamp;
            realtime_context_.receiver_buffer.set_next_ts(packet->timestamp);
            realtime_context_.next_ts = packet_timestamp;
        }

        // Determine whether whole packet is too old
        if (packet_timestamp + packet->packet_time_frames <= realtime_context_.next_ts) {
            RAV_WARNING("Packet too late: seq={}, ts={}", packet->seq, packet->timestamp);
            TRACY_MESSAGE("Packet too late - skipping");
            realtime_context_.packets_too_old.push(packet->seq);
            continue;
        }

        // Determine whether packet contains outdated data
        if (packet_timestamp < realtime_context_.next_ts) {
            RAV_WARNING("Packet partly too late: seq={}, ts={}", packet->seq, packet->timestamp);
            TRACY_MESSAGE("Packet partly too late - not skipping");
            realtime_context_.packets_too_old.push(packet->seq);
            // Still process the packet since it contains data that is not outdated
        }

        realtime_context_.receiver_buffer.clear_until(packet->timestamp);

        if (!realtime_context_.receiver_buffer.write(packet->timestamp, {packet->data.data(), packet->data_len})) {
            RAV_ERROR("Packet not written to buffer");
        }
    }

    TRACY_PLOT(
        "available_frames",
        static_cast<int64_t>(realtime_context_.next_ts.diff(realtime_context_.receiver_buffer.next_ts()))
    );

    realtime_context_.next_ts = at_timestamp + num_frames;
    return realtime_context_.receiver_buffer.read(at_timestamp, buffer, buffer_size);
}

rav::rtp_stream_receiver::stream_stats rav::rtp_stream_receiver::get_session_stats() const {
    if (streams_.empty()) {
        return {};
    }
    stream_stats s;
    const auto& stream = streams_.front();
    s.packet_stats = stream.packet_stats.get_total_counts();
    s.packet_interval_stats = stream.packet_interval_stats.get_stats();
    return s;
}

rav::rtp_packet_stats::counters rav::rtp_stream_receiver::get_packet_stats() const {
    if (streams_.empty()) {
        return {};
    }
    return streams_.front().packet_stats.get_total_counts();
}

rav::sliding_stats::stats rav::rtp_stream_receiver::get_packet_interval_stats() const {
    if (streams_.empty()) {
        return {};
    }
    return streams_.front().packet_interval_stats.get_stats();
}

void rav::rtp_stream_receiver::restart() {
    if (!selected_format_.is_valid()) {
        return;
    }

    rtp_receiver_.unsubscribe(*this);  // This unsubscribes `this` from all sessions

    const auto bytes_per_frame = selected_format_.bytes_per_frame();
    RAV_ASSERT(bytes_per_frame > 0, "bytes_per_frame must be greater than 0");

    realtime_context_.receiver_buffer.resize(std::max(1024u, delay_ * k_delay_multiplier), bytes_per_frame);
    realtime_context_.fifo.resize(delay_);  // TODO: Determine sensible size (maybe this is the sensible size)
    realtime_context_.packets_too_old.resize(delay_);  // TODO: Determine sensible size
    realtime_context_.selected_audio_format = selected_format_;

    for (auto& stream : streams_) {
        rtp_receiver_.subscribe(*this, stream.session, stream.filter);
        stream.first_packet_timestamp.reset();
        stream.packet_stats.reset();
    }

    RAV_TRACE("(Re)Started rtp_stream_receiver");
}

rav::rtp_stream_receiver::stream_state&
rav::rtp_stream_receiver::find_or_create_stream_info(const rtp_session& session) {
    for (auto& stream : streams_) {
        if (stream.session == session) {
            return stream;
        }
    }

    return streams_.emplace_back(session);
}

void rav::rtp_stream_receiver::handle_rtp_packet_event_for_stream(
    const rtp_receiver::rtp_packet_event& event, stream_state& stream
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

    if (consumer_active_) {
        intermediate_packet intermediate {};
        intermediate.timestamp = event.packet.timestamp();
        intermediate.seq = event.packet.sequence_number();
        intermediate.data_len = static_cast<uint16_t>(payload.size_bytes());
        intermediate.packet_time_frames = stream.packet_time_frames;
        std::memcpy(intermediate.data.data(), payload.data(), intermediate.data_len);

        if (!realtime_context_.fifo.push(intermediate)) {
            RAV_TRACE("Failed to push packet info FIFO, make receiver inactive");
            consumer_active_ = false;
            return;
        }
    }

    while (auto seq = realtime_context_.packets_too_old.pop()) {
        stream.packet_stats.mark_packet_too_late(*seq);
    }

    if (const auto stats = stream.packet_stats.update(event.packet.sequence_number())) {
        if (auto v = stream.packet_stats_throttle.update(*stats)) {
            RAV_WARNING("Stats for stream {}: {}", stream.session.to_string(), v->to_string());
        }
    }

    if (const auto diff = stream.seq.update(event.packet.sequence_number())) {
        if (diff >= 1) {
            // Only call back with monotonically increasing sequence numbers
            for (const auto& s : subscribers_) {
                s->on_data_received(packet_timestamp);
            }
        }

        if (packet_timestamp - delay_ >= *stream.first_packet_timestamp) {
            // Make sure to call with the correct timestamps for the missing packets
            for (uint16_t i = 0; i < *diff; ++i) {
                for (const auto& s : subscribers_) {
                    s->on_data_ready(packet_timestamp - delay_ - (*diff - 1u - i) * stream.packet_time_frames);
                }
            }
        }
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

    for (auto& stream : streams_) {
        if (rtp_event.session == stream.session) {
            handle_rtp_packet_event_for_stream(rtp_event, stream);
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
