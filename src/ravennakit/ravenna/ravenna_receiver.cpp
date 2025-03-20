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
#include "ravennakit/rtp/detail/rtp_filter.hpp"

rav::ravenna_receiver::ravenna_receiver(ravenna_rtsp_client& rtsp_client, rtp::rtp_receiver& rtp_receiver) :
    rtp_stream_receiver(rtp_receiver), rtsp_client_(rtsp_client) {}

rav::ravenna_receiver::~ravenna_receiver() {
    std::ignore = rtsp_client_.unsubscribe_from_all_sessions(this);
}

void rav::ravenna_receiver::on_announced(const ravenna_rtsp_client::announced_event& event) {
    try {
        RAV_ASSERT(event.session_name == session_name_, "Expecting session_name to match");
        update_sdp(event.sdp);
        RAV_TRACE("SDP updated for session '{}'", session_name_);
    } catch (const std::exception& e) {
        RAV_ERROR("Failed to process SDP for session '{}': {}", session_name_, e.what());
    }
}

bool rav::ravenna_receiver::subscribe_to_session(const std::string& session_name) {
    if (session_name_ == session_name) {
        return false;
    }
    if (!session_name_.empty()) {
        if (!rtsp_client_.unsubscribe_from_all_sessions(this)) {
            RAV_ERROR("Failed to unsubscribe from all sessions");
        }
    }
    session_name_ = session_name;
    if (!session_name_.empty()) {
        if (!rtsp_client_.subscribe_to_session(this, session_name_)) {
            RAV_ERROR("Failed to subscribe to session '{}'", session_name_);
            return false;
        }
    }
    return true;
}

const std::string& rav::ravenna_receiver::get_session_name() const {
    return session_name_;
}

std::optional<rav::sdp::session_description> rav::ravenna_receiver::get_sdp() const {
    return rtsp_client_.get_sdp_for_session(session_name_);
}

std::optional<std::string> rav::ravenna_receiver::get_sdp_text() const {
    return rtsp_client_.get_sdp_text_for_session(session_name_);
}
