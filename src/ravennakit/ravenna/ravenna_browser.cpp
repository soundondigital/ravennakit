/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_browser.hpp"

#include "ravennakit/core/log.hpp"

rav::ravenna_browser::ravenna_browser(asio::io_context& io_context) : io_context_(io_context) {
    node_browser_ = dnssd::dnssd_browser::create(io_context_);
    if (node_browser_ != nullptr) {
        node_browser_->on<dnssd::dnssd_service_resolved>([](const dnssd::dnssd_service_resolved& event) {
            RAV_INFO("RAVENNA Node resolved: {}", event.description.name);
        });

        // Browse for RAVENNA nodes (note the subtype _ravenna)
        node_browser_->browse_for("_rtsp._tcp,_ravenna");
    }

    session_browser_ = dnssd::dnssd_browser::create(io_context_);
    if (session_browser_ != nullptr) {
        session_browser_->on<dnssd::dnssd_service_resolved>([this](const dnssd::dnssd_service_resolved& event) {
            RAV_INFO("RAVENNA Stream resolved: {}", event.description.name);

            subscribers_.foreach([&event](auto& s) {
                s.emit(ravenna_session_resolved {event.description});
            });
        });

        // Browse for RAVENNA sessions (note the subtype _ravenna_session)
        session_browser_->browse_for("_rtsp._tcp,_ravenna_session");
    }
}

void rav::ravenna_browser::subscribe(subscriber& s) {
    subscribers_.push_back(s);
}
