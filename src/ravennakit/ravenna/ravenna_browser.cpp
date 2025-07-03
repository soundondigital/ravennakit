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

rav::RavennaBrowser::RavennaBrowser(boost::asio::io_context& io_context) {
    node_browser_ = dnssd::Browser::create(io_context);

    if (node_browser_ == nullptr) {
        RAV_THROW_EXCEPTION("No dnssd browser available");
    }

    session_browser_ = dnssd::Browser::create(io_context);

    if (session_browser_ == nullptr) {
        RAV_THROW_EXCEPTION("No dnssd browser available");
    }

    node_browser_->on_service_resolved = [this](const auto& desc) {
        for (auto* s : subscribers_) {
            s->ravenna_node_discovered(desc);
        }
    };
    node_browser_->on_service_removed = [this](const auto& desc) {
        for (auto* s : subscribers_) {
            s->ravenna_node_removed(desc);
        }
    };

    session_browser_->on_service_resolved = [this](const auto& desc) {
        for (auto* s : subscribers_) {
            s->ravenna_session_discovered(desc);
        }
    };
    session_browser_->on_service_removed = [this](const auto& desc) {
        for (auto* s : subscribers_) {
            s->ravenna_session_removed(desc);
        }
    };

    node_browser_->browse_for("_rtsp._tcp,_ravenna");
    session_browser_->browse_for("_rtsp._tcp,_ravenna_session");
}

const rav::dnssd::ServiceDescription* rav::RavennaBrowser::find_session(const std::string& session_name) const {
    RAV_ASSERT(node_browser_ != nullptr, "Invalid node browser");
    RAV_ASSERT(session_browser_ != nullptr, "Invalid session browser");
    return session_browser_->find_service(session_name);
}

const rav::dnssd::ServiceDescription* rav::RavennaBrowser::find_node(const std::string& node_name) const {
    RAV_ASSERT(node_browser_ != nullptr, "Invalid node browser");
    RAV_ASSERT(session_browser_ != nullptr, "Invalid session browser");
    return node_browser_->find_service(node_name);
}

bool rav::RavennaBrowser::subscribe(Subscriber* subscriber_to_add) {
    if (subscribers_.add(subscriber_to_add)) {
        for (auto& s : node_browser_->get_services()) {
            if (s.host_target.empty()) {
                continue;  // Only consider resolved services
            }
            subscriber_to_add->ravenna_node_discovered({s});
        }

        for (auto& s : session_browser_->get_services()) {
            if (s.host_target.empty()) {
                continue;  // Only consider resolved services
            }
            subscriber_to_add->ravenna_session_discovered({s});
        }
        return true;
    }
    return false;
}

bool rav::RavennaBrowser::unsubscribe(const Subscriber* subscriber_to_remove) {
    return subscribers_.remove(subscriber_to_remove);
}
