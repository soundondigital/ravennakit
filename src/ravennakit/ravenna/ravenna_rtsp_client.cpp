/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_rtsp_client.hpp"

rav::ravenna_rtsp_client::ravenna_rtsp_client(asio::io_context& io_context, ravenna_browser& browser) :
    io_context_(io_context) {
    ravenna_browser_subscriber_->on<ravenna_session_resolved>([](const ravenna_session_resolved& event) {
        RAV_INFO("RAVENNA Stream resolved: {}", event.description.name);
    });
    browser.subscribe(ravenna_browser_subscriber_);
}

void rav::ravenna_rtsp_client::subscribe(const std::string& session_name, subscriber& s) {
    if (session_name.empty()) {
        RAV_THROW_EXCEPTION("session_name cannot be empty");
    }

    auto& session = sessions_[session_name];
    session.subscribers.push_back(s);
    if (session.sdp_) {
        s->emit(sdp_updated{session_name, *session.sdp_});
    }

    // TODO: See if there is connection info available on the browser, if so use that to create a connection.
    // Use the connection to fetch the SDP.
    // If no connection info is available, wait for the browser to provide connection info.
    // If connection info comes in, see if there are sessions which need to be resolved.
}
