/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_sink.hpp"
#include "ravennakit/core/todo.hpp"
#include "ravennakit/ravenna/ravenna_constants.hpp"
#include "ravennakit/rtp/detail/rtp_filter.hpp"

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

rav::ravenna_sink::ravenna_sink(
    ravenna_rtsp_client& rtsp_client, rtp_receiver& rtp_receiver, std::string session_name
) :
    rtsp_client_(rtsp_client), rtp_receiver_(rtp_receiver) {
    set_session_name(std::move(session_name));
}

rav::ravenna_sink::~ravenna_sink() {
    rtp_receiver_.unsubscribe(*this);
    stop();
}

void rav::ravenna_sink::on([[maybe_unused]] const rtp_receiver::rtp_packet_event& rtp_event) {}

void rav::ravenna_sink::on([[maybe_unused]] const rtp_receiver::rtcp_packet_event& rtcp_event) {}

void rav::ravenna_sink::on(const ravenna_rtsp_client::announced_event& event) {
    try {
        RAV_ASSERT(event.session_name == session_name_, "Expecting session_name to match");
        RAV_TRACE("SDP updated for session '{}'", session_name_);

        const auto& sdp = event.sdp;

        // Try to find a suitable media description
        const sdp::media_description* found_media_description = nullptr;
        const sdp::format* found_format = nullptr;
        const sdp::connection_info_field* found_connection_info = nullptr;
        for (auto& media_description : sdp.media_descriptions()) {
            if (media_description.media_type() != "audio") {
                RAV_WARNING(
                    "Unsupported media type '{}' in SDP for session '{}'", media_description.media_type(), session_name_
                );
                continue;
            }

            if (media_description.protocol() != "RTP/AVP") {
                RAV_WARNING(
                    "Unsupported protocol '{}' in SDP for session '{}'", media_description.protocol(), session_name_
                );
                continue;
            }

            // The first acceptable payload format from the beginning of the list SHOULD be used for the session.
            // https://datatracker.ietf.org/doc/html/rfc8866#name-media-descriptions-m
            const auto& names = ravenna::constants::k_supported_rtp_encoding_names;
            for (auto& format : media_description.formats()) {
                if (std::find(names.begin(), names.end(), format.encoding_name) == names.end()) {
                    RAV_WARNING(
                        "Unsupported encoding name '{}' in SDP for session '{}'", format.encoding_name, session_name_
                    );
                    continue;
                }
                found_format = &format;
                break;
            }

            if (found_format == nullptr) {
                RAV_WARNING("No suitable format in SDP for session '{}'", session_name_);
                continue;
            }

            for (auto& conn : media_description.connection_infos()) {
                if (!is_connection_info_valid(conn)) {
                    continue;
                }
                found_connection_info = &conn;
            }

            found_media_description = &media_description;
            break;
        }

        if (!found_media_description) {
            RAV_WARNING("No suitable media description found in SDP for session '{}'", session_name_);
            return;
        }

        RAV_ASSERT(found_media_description != nullptr, "Expecting found_media_description to be set");
        RAV_ASSERT(found_format != nullptr, "Expecting found_format to be set");

        if (found_connection_info == nullptr) {
            if (const auto& conn = sdp.connection_info()) {
                if (is_connection_info_valid(*conn)) {
                    found_connection_info = &*conn;
                }
            }
        }

        if (found_connection_info == nullptr) {
            RAV_WARNING("No suitable connection info found in SDP for session '{}'", session_name_);
            return;
        }

        RAV_ASSERT(found_connection_info != nullptr, "Expecting found_connection_info to be set");

        auto connection_address = asio::ip::make_address(found_connection_info->address);

        rtp_filter filter(connection_address);

        const auto& source_filters = found_media_description->source_filters();
        if (!source_filters.empty()) {
            if (filter.add_filters(source_filters) == 0) {
                RAV_WARNING("No suitable source filters found in SDP for session '{}'", session_name_);
            }
        } else {
            const auto& sdp_source_filters = sdp.source_filters();
            if (!sdp_source_filters.empty()) {
                if (filter.add_filters(sdp_source_filters) == 0) {
                    RAV_WARNING("No suitable source filters found in SDP for session '{}'", session_name_);
                }
            }
        }

        settings new_settings;

        new_settings.session.connection_address = std::move(connection_address);
        new_settings.format = *found_format;
        new_settings.session.rtp_port = found_media_description->port();
        new_settings.session.rtcp_port = new_settings.session.rtp_port + 1;
        new_settings.filter = std::move(filter);

        // Now we have a suitable media description
        RAV_TRACE("Found suitable media description in SDP for session '{}'", session_name_);

        rtp_receiver_.subscribe(*this, new_settings.session);

        current_settings_ = std::move(new_settings);
    } catch (const std::exception& e) {
        RAV_ERROR("Failed to process SDP for session '{}': {}", session_name_, e.what());
    }
}

void rav::ravenna_sink::start() {
    if (started_) {
        RAV_WARNING("ravenna_sink already started");
        return;
    }

    subscribe_to_ravenna_rtsp_client(rtsp_client_, session_name_);

    started_ = true;
}

void rav::ravenna_sink::stop() {
    unsubscribe_from_ravenna_rtsp_client();
    started_ = false;
}

void rav::ravenna_sink::set_session_name(std::string session_name) {
    session_name_ = std::move(session_name);
    subscribe_to_ravenna_rtsp_client(rtsp_client_, session_name_);
}
