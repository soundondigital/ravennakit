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

rav::ravenna_browser::ravenna_browser(asio::io_context& io_context) {
    node_browser_ = dnssd::dnssd_browser::create(io_context);
    if (node_browser_ != nullptr) {
        node_browser_->on<dnssd::events::service_resolved>([](const rav::dnssd::events::service_resolved& event,
                                                              rav::dnssd::dnssd_browser&) {
            RAV_INFO("RAVENNA Node resolved: {}", event.description.name);
        });

        // Browse for RAVENNA nodes (note the subtype _ravenna)
        node_browser_->browse_for("_rtsp._tcp,_ravenna");
    }

    session_browser_ = dnssd::dnssd_browser::create(io_context);
    if (session_browser_ != nullptr) {
        session_browser_->on<rav::dnssd::events::service_resolved>([](const rav::dnssd::events::service_resolved& event,
                                                              rav::dnssd::dnssd_browser&) {
            RAV_INFO("RAVENNA Stream resolved: {}", event.description.name);
        });

        // Browse for RAVENNA streams (note the subtype _ravenna_session)
        session_browser_->browse_for("_rtsp._tcp,_ravenna_session");
    }
}
