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

rav::ravenna_sink::ravenna_sink(
    ravenna_rtsp_client& rtsp_client, rtp_receiver& rtp_receiver, std::string session_name
) :
    rtsp_client_(rtsp_client), rtp_receiver_(rtp_receiver) {
    set_source(std::move(session_name));
}

rav::ravenna_sink::~ravenna_sink() {
    stop();
}

void rav::ravenna_sink::set_manual_sdp(sdp::session_description sdp) {
    manual_sdp_ = std::move(sdp);
}

void rav::ravenna_sink::on(const rtp_receiver::rtp_packet_event& rtp_event) {}

void rav::ravenna_sink::on(const rtp_receiver::rtcp_packet_event& rtcp_event) {
    RAV_TRACE("{}", rtcp_event.packet.to_string());
}

void rav::ravenna_sink::on(const ravenna_rtsp_client::announced_event& event) {
    RAV_ASSERT(event.session_name == session_name_, "Expecting session_name to match");
    auto_sdp_ = event.sdp;
    RAV_TRACE("SDP updated for session '{}'", session_name_);

    auto& sdp = event.sdp;
    if (sdp.media_descriptions().empty()) {
        RAV_WARNING("No media descriptions in SDP for session '{}'", session_name_);
        return;
    }

    // Try to find a suitable media description
    for (auto& media_description : sdp.media_descriptions()) {
        if (media_description.media_type() != "audio") {
            RAV_TRACE(
                "Unsupported media type '{}' in SDP for session '{}'", media_description.media_type(), session_name_
            );
            continue;
        }

        // At this moment we only support the default port. In the future we may support other ports.
        if (media_description.port() != ravenna::constants::k_default_rtp_port) {
            RAV_WARNING("Unsupported port '{}' in SDP for session '{}'", media_description.port(), session_name_);
            return;
        }

    }


    auto& media_description = sdp.media_descriptions().front();



    if (media_description.protocol() != "RTP/AVP") {
        RAV_WARNING("Unsupported protocol '{}' in SDP for session '{}'", media_description.protocol(), session_name_);
        return;
    }

    if (media_description.formats().empty()) {
        RAV_WARNING("No formats in SDP for session '{}'", session_name_);
        return;
    }

    auto& fmt = media_description.formats().front();

    const auto& names = ravenna::constants::k_supported_rtp_encoding_names;
    if (std::find(names.begin(), names.end(), fmt.encoding_name) == names.end()) {
        RAV_WARNING("Unsupported encoding name '{}' in SDP for session '{}'", fmt.encoding_name, session_name_);
        return;
    }

    sdp::connection_info_field connection_info;

    if (!media_description.connection_infos().empty()) {
        connection_info = media_description.connection_infos().front();
    } else if (auto info = sdp.connection_info()) {
        connection_info = std::move(info.value());
    } else {
        RAV_WARNING("No connection info in SDP for session '{}'", session_name_);
        return;
    }

    if (!media_description.source_filters().empty()) {
        // TODO: Implement source filters
    }

    asio::error_code ec;
    const auto connection_address = asio::ip::make_address(connection_info.address, ec);
    if (ec) {
        RAV_WARNING("Failed to create address from '{}': {}", connection_info.address, ec.message());
        return;
    }

    // TODO: Compile a list of ip destinations and sources from the SDP
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

void rav::ravenna_sink::set_mode(const mode new_mode) {
    mode_ = new_mode;
}

void rav::ravenna_sink::set_source(std::string session_name) {
    session_name_ = std::move(session_name);
    subscribe_to_ravenna_rtsp_client(rtsp_client_, session_name_);
}
