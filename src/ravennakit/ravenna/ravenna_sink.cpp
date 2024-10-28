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

rav::ravenna_sink::ravenna_sink(ravenna_rtsp_client& rtsp_client, std::string session_name) :
    rtsp_client_(rtsp_client) {
    rtsp_subscriber_->on<ravenna_rtsp_client::sdp_updated>([this](const ravenna_rtsp_client::sdp_updated& event) {
        RAV_ASSERT(event.session_name == session_name_, "Expecting session_name to match");
        auto_sdp_ = event.sdp;
        RAV_INFO("SDP updated for session '{}'", session_name_);
    });

    set_source(std::move(session_name));
}

void rav::ravenna_sink::set_sdp(sdp::session_description sdp) {
    manual_sdp_ = std::move(sdp);
}

void rav::ravenna_sink::set_mode(const mode new_mode) {
    mode_ = new_mode;
}

void rav::ravenna_sink::set_source(std::string session_name) {
    session_name_ = std::move(session_name);
    rtsp_client_.subscribe(session_name_, rtsp_subscriber_);

    TODO("Implement");
}
