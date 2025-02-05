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

    uint32_t packet_time_frames = 0;
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
    stream.packet_stats.reset(
        std::ceil(
            k_stats_window_size_ms * static_cast<float>(selected_audio_format->sample_rate) / 1000.0
            / packet_time_frames
        )
    );

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

bool rav::rtp_stream_receiver::read_data(const uint32_t at_timestamp, uint8_t* buffer, const size_t buffer_size) const {
    return receiver_buffer_.read(at_timestamp, buffer, buffer_size);
}

void rav::rtp_stream_receiver::restart() {
    if (!selected_format_.is_valid()) {
        return;
    }

    rtp_receiver_.unsubscribe(*this);  // This unsubscribes `this` from all sessions

    const auto bytes_per_frame = selected_format_.bytes_per_frame();
    RAV_ASSERT(bytes_per_frame > 0, "bytes_per_frame must be greater than 0");

    receiver_buffer_.resize(std::max(1024u, delay_ * k_delay_multiplier), bytes_per_frame);

    for (auto& stream : streams_) {
        rtp_receiver_.subscribe(*this, stream.session, stream.filter);
        stream.first_packet_timestamp.reset();
        stream.packet_stats.reset();
    }

    RAV_TRACE("(Re)Started rtp_stream_receiver");
}

rav::rtp_stream_receiver::stream_info&
rav::rtp_stream_receiver::find_or_create_stream_info(const rtp_session& session) {
    for (auto& stream : streams_) {
        if (stream.session == session) {
            return stream;
        }
    }

    return streams_.emplace_back(session);
}

void rav::rtp_stream_receiver::handle_rtp_packet_for_stream(const rtp_packet_view& packet, stream_info& stream) {
    TRACY_ZONE_SCOPED;

    const wrapping_uint32 packet_timestamp(packet.timestamp());

    if (!stream.first_packet_timestamp.has_value()) {
        receiver_buffer_.set_next_ts(packet.timestamp());
        stream.seq = packet.sequence_number();
        stream.first_packet_timestamp = packet.timestamp();
    }

    receiver_buffer_.clear_until(packet.timestamp());

    // Discard packet if it's too old
    if (packet_timestamp + stream.packet_time_frames < receiver_buffer_.next_ts() - delay_) {
        // TODO: The age should be determined by the consuming side, not the producing side
        RAV_WARNING(
            "Dropping packet because it's too old (ts: {}, next_ts: {})", packet.timestamp(),
            receiver_buffer_.next_ts().value()
        );
        return;
    }

    if (!receiver_buffer_.write(packet.timestamp(), packet.payload_data())) {
        RAV_ERROR("Packet not written to buffer");
    }

    TRACY_PLOT("RTP Timestamp", static_cast<int64_t>(packet.timestamp()));

    if (const auto counters = stream.packet_stats.update(packet.sequence_number())) {
        if (auto v = stream.packet_stats_throttle.update(*counters)) {
            RAV_TRACE("Stats for stream {}: {}", stream.session.to_string(), v->to_string());
        }
    }

    if (const auto step = stream.seq.update(packet.sequence_number())) {
        if (step >= 1) {
            if (packet_timestamp - delay_ >= *stream.first_packet_timestamp) {
                for (const auto& s : subscribers_) {
                    s->on_data_available(packet_timestamp - delay_);
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
            handle_rtp_packet_for_stream(rtp_event.packet, stream);
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
