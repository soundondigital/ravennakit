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
    const sdp::format* selected_format = nullptr;

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
        if (media_description.formats().empty()) {
            RAV_WARNING("No formats in SDP");
            continue;
        }

        // TODO: Query subclass for supported formats (by looping the available formats)
        selected_format = &media_description.formats().front();

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

    RAV_ASSERT(selected_media_description != nullptr, "Expecting found_media_description to be set");
    RAV_ASSERT(selected_format != nullptr, "Expecting found_format to be set");

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
    auto ptime = selected_media_description->ptime();
    if (ptime.has_value()) {
        packet_time_frames = static_cast<uint32_t>(std::round(*ptime * selected_format->clock_rate)) / 1000;
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

    auto& stream_info = find_or_create_stream_info(session);

    // Session
    if (stream_info.session != session) {
        should_restart = true;
        stream_info.session = session;
    }
    stream_info.session = session;

    // Filter
    stream_info.filter = filter;  // TODO: Is a restart required if the filter changes?

    if (selected_format_ != *selected_format) {
        should_restart = true;
        selected_format_ = *selected_format;
        RAV_TRACE("Format changed from {} to {}", selected_format_, *selected_format);
    }

    // TODO: Remove stream_infos which are not in the SDP anymore. If there are such sessions we would probably also
    // want to do a restart.

    if (should_restart) {
        restart();
        RAV_TRACE("Restarted rtp_stream_receiver");
    }
}

void rav::rtp_stream_receiver::set_delay(const uint32_t delay) {
    if (delay == delay_) {
        return;
    }
    delay_ = delay;
    restart();
}

void rav::rtp_stream_receiver::restart() {
    rtp_receiver_.unsubscribe(*this);  // This unsubscribes this from all sessions

    const auto bytes_per_frame = selected_format_.bytes_per_sample();
    if (!bytes_per_frame.has_value()) {
        RAV_ERROR("Unable to determine bytes per frame for format {}", selected_format_.to_string());
        return;
    }

    receiver_buffer_.resize(delay_ * k_delay_multiplier, bytes_per_frame.value());

    for (auto& stream : streams_) {
        rtp_receiver_.subscribe(*this, stream.session, stream.filter);
    }
}

rav::rtp_stream_receiver::stream_info&
rav::rtp_stream_receiver::find_or_create_stream_info(const rtp_session& session) {
    for (auto& stream : streams_) {
        if (stream.session == session) {
            return stream;
        }
    }

    return streams_.emplace_back(session, rtp_depacketizer(receiver_buffer_));
}

void rav::rtp_stream_receiver::on_rtp_packet(const rtp_receiver::rtp_packet_event& rtp_event) {
    // TODO: We should probably discard filtered packets here and not in rtp_receiver. This would also allow us to use a
    // subscriber list without context in rtp_receiver. Alternatively we could add a virtual function to
    // rtp_receiver::subscriber to determine whether the packet should be filtered or not. But since we need to call a
    // virtual function anyway (this one) we might as well filter it here.

    RAV_TRACE(
        "{} for session {} from {}:{}", rtp_event.packet.to_string(), rtp_event.session.to_string(),
        rtp_event.src_endpoint.address().to_string(), rtp_event.src_endpoint.port()
    );

    for (auto& stream : streams_) {
        if (rtp_event.session == stream.session) {
            stream.depacketizer.handle_rtp_packet(rtp_event.packet, delay_);
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
