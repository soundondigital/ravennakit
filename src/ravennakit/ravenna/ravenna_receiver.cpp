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

rav::ravenna_receiver::ravenna_receiver(
    ravenna_rtsp_client& rtsp_client, rtp_receiver& rtp_receiver, std::string session_name
) :
    rtp_stream_receiver(rtp_receiver), rtsp_client_(rtsp_client) {
    set_session_name(std::move(session_name));
}

rav::ravenna_receiver::~ravenna_receiver() {
    stop();
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

void rav::ravenna_receiver::start() {
    if (started_) {
        RAV_WARNING("ravenna_sink already started");
        return;
    }

    subscribe_to_ravenna_rtsp_client(rtsp_client_, session_name_);

    started_ = true;
}

void rav::ravenna_receiver::stop() {
    unsubscribe_from_ravenna_rtsp_client();
    started_ = false;
}

void rav::ravenna_receiver::set_session_name(std::string session_name) {
    session_name_ = std::move(session_name);
    subscribe_to_ravenna_rtsp_client(rtsp_client_, session_name_);
}

std::string rav::ravenna_receiver::get_session_name() const {
    return session_name_;
}
