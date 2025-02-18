/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/ravenna/ravenna_browser.hpp"
#include "ravennakit/core/exception.hpp"
#include "ravennakit/core/log.hpp"

rav::ravenna_browser::ravenna_browser(asio::io_context& io_context) {
    node_browser_ = dnssd::dnssd_browser::create(io_context);

    if (node_browser_ == nullptr) {
        RAV_THROW_EXCEPTION("No dnssd browser available");
    }

    session_browser_ = dnssd::dnssd_browser::create(io_context);

    if (session_browser_ == nullptr) {
        RAV_THROW_EXCEPTION("No dnssd browser available");
    }

    node_browser_subscriber_->on<dnssd::dnssd_browser::service_resolved>([this](const auto& event) {
        for (auto* s : subscribers_) {
            s->ravenna_node_discovered(event);
        }
    });
    node_browser_subscriber_->on<dnssd::dnssd_browser::service_removed>([this](const auto& event) {
        for (auto* s : subscribers_) {
            s->ravenna_node_removed(event);
        }
    });
    node_browser_->subscribe(node_browser_subscriber_);

    session_browser_subscriber_->on<dnssd::dnssd_browser::service_resolved>([this](const auto& event) {
        for (auto* s : subscribers_) {
            s->ravenna_session_discovered(event);
        }
    });
    session_browser_subscriber_->on<dnssd::dnssd_browser::service_removed>([this](const auto& event) {
        for (auto* s : subscribers_) {
            s->ravenna_session_removed(event);
        }
    });
    session_browser_->subscribe(session_browser_subscriber_);

    node_browser_->browse_for("_rtsp._tcp,_ravenna");
    session_browser_->browse_for("_rtsp._tcp,_ravenna_session");
}

void rav::ravenna_browser::subscribe(subscriber* subscriber) {
    RAV_ASSERT(subscriber != nullptr, "Invalid subscriber");
    RAV_ASSERT(node_browser_ != nullptr, "Invalid node browser");
    RAV_ASSERT(session_browser_ != nullptr, "Invalid session browser");

    if (!subscribers_.add(subscriber)) {
        RAV_WARNING("Subscriber already exists");
        return;
    }

    for (auto& s : node_browser_->get_services()) {
        if (s.host_target.empty()) {
            continue; // Only consider resolved services
        }
        subscriber->ravenna_node_discovered({s});
    }

    for (auto& s : session_browser_->get_services()) {
        if (s.host_target.empty()) {
            continue; // Only consider resolved services
        }
        subscriber->ravenna_session_discovered({s});
    }
}

void rav::ravenna_browser::unsubscribe(subscriber* subscriber) {
    RAV_ASSERT(subscriber != nullptr, "Invalid subscriber");
    RAV_ASSERT(node_browser_ != nullptr, "Invalid node browser");
    RAV_ASSERT(session_browser_ != nullptr, "Invalid session browser");

    if (!subscribers_.remove(subscriber)) {
        RAV_WARNING("Subscriber not found");
    }
}
