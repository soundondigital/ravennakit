/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_receiver.hpp"
#include "ravennakit/core/util/todo.hpp"
#include "ravennakit/ravenna/ravenna_constants.hpp"
#include "ravennakit/rtp/detail/rtp_filter.hpp"

rav::ravenna_receiver::ravenna_receiver(ravenna_rtsp_client& rtsp_client, rtp_receiver& rtp_receiver) :
    rtp_stream_receiver(rtp_receiver), rtsp_client_(rtsp_client) {
    set_ravenna_rtsp_client(&rtsp_client_);
}

rav::ravenna_receiver::~ravenna_receiver() {
    set_ravenna_rtsp_client(nullptr);
}

void rav::ravenna_receiver::on_announced(const ravenna_rtsp_client::announced_event& event) {
    try {
        RAV_ASSERT(event.session_name == get_session_name(), "Expecting session_name to match");
        update_sdp(event.sdp);
        RAV_TRACE("SDP updated for session '{}'", get_session_name());
    } catch (const std::exception& e) {
        RAV_ERROR("Failed to process SDP for session '{}': {}", get_session_name(), e.what());
    }
}

std::optional<rav::sdp::session_description> rav::ravenna_receiver::get_sdp() const {
    return rtsp_client_.get_sdp_for_session(get_session_name());
}

std::optional<std::string> rav::ravenna_receiver::get_sdp_text() const {
    return rtsp_client_.get_sdp_text_for_session(get_session_name());
}
