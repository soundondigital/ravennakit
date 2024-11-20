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
    reset();
}

void rav::rtp_stream_receiver::update_sdp(const sdp::session_description& sdp) {
    const sdp::media_description* primary_media_description = nullptr;
    const sdp::connection_info_field* primary_connection_info = nullptr;
    const sdp::format* found_format = nullptr;

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
        found_format = &media_description.formats().front();

        for (auto& conn : media_description.connection_infos()) {
            if (!is_connection_info_valid(conn)) {
                continue;
            }
            primary_connection_info = &conn;
        }

        primary_media_description = &media_description;
        break;
    }

    if (!primary_media_description) {
        RAV_WARNING("No suitable media description found in SDP");
        return;
    }

    RAV_ASSERT(primary_media_description != nullptr, "Expecting found_media_description to be set");
    RAV_ASSERT(found_format != nullptr, "Expecting found_format to be set");

    if (primary_connection_info == nullptr) {
        if (const auto& conn = sdp.connection_info()) {
            if (is_connection_info_valid(*conn)) {
                primary_connection_info = &*conn;
            }
        }
    }

    if (primary_connection_info == nullptr) {
        RAV_WARNING("No suitable connection info found in SDP");
        return;
    }

    RAV_ASSERT(primary_connection_info != nullptr, "Expecting found_connection_info to be set");

    session_info primary;
    primary.session.connection_address = asio::ip::make_address(primary_connection_info->address);
    primary.session.rtp_port = primary_media_description->port();
    primary.session.rtcp_port = primary.session.rtp_port + 1;
    primary.filter = rtp_filter(primary.session.connection_address);

    const auto& source_filters = primary_media_description->source_filters();
    if (!source_filters.empty()) {
        if (primary.filter.add_filters(source_filters) == 0) {
            RAV_WARNING("No suitable source filters found in SDP");
        }
    } else {
        const auto& sdp_source_filters = sdp.source_filters();
        if (!sdp_source_filters.empty()) {
            if (primary.filter.add_filters(sdp_source_filters) == 0) {
                RAV_WARNING("No suitable source filters found in SDP");
            }
        }
    }

    bool should_reset = false;

    if (expected_format_ && *expected_format_ != *found_format) {
        should_reset = true;
        RAV_TRACE("Format changed from {} to {}", *expected_format_, *found_format);
    }

    if (primary_session_info_ && primary_session_info_->session != primary.session) {
        should_reset = true;
        RAV_TRACE(
            "Primary session info changed changed from {} to {}", primary_session_info_->session.to_string(),
            primary.session.to_string()
        );
    }

    // TODO: Add and check secondary connection info

    if (should_reset) {
        reset();
    }

    expected_format_ = *found_format;
    primary_session_info_ = std::move(primary);

    // Now we have a suitable media description
    RAV_TRACE("Found suitable media description in SDP");

    rtp_receiver_.subscribe(*this, primary_session_info_->session, primary_session_info_->filter);
}

void rav::rtp_stream_receiver::reset() const {
    rtp_receiver_.unsubscribe(*this);
}

void rav::rtp_stream_receiver::on_rtp_packet(const rtp_receiver::rtp_packet_event& rtp_event) {
    RAV_TRACE(
        "{} from {}:{} to {}:{}", rtp_event.packet.to_string(), rtp_event.src_endpoint.address().to_string(),
        rtp_event.src_endpoint.port(), rtp_event.dst_endpoint.address().to_string(), rtp_event.dst_endpoint.port()
    );
}

void rav::rtp_stream_receiver::on_rtcp_packet(const rtp_receiver::rtcp_packet_event& rtcp_event) {
    RAV_TRACE(
        "{} from {}:{} to {}:{}", rtcp_event.packet.to_string(), rtcp_event.src_endpoint.address().to_string(),
        rtcp_event.src_endpoint.port(), rtcp_event.dst_endpoint.address().to_string(), rtcp_event.dst_endpoint.port()
    );
}
