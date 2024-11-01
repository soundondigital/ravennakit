/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_node.hpp"

rav::ravenna_node::ravenna_node() {
    node_browser_ = dnssd::dnssd_browser::create(io_context_);
    session_browser_ = dnssd::dnssd_browser::create(io_context_);
    // ravenna_rtsp_client_ = std::make_unique<ravenna_rtsp_client>(io_context_, *session_browser_);

    node_browser_->browse_for("_rtsp._tcp,_ravenna");
    session_browser_->browse_for("_rtsp._tcp,_ravenna_session");
}

void rav::ravenna_node::create_sink(const std::string& stream_name) {
    std::ignore = stream_name;
}
