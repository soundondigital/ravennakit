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
#include "ravennakit/core/expected.hpp"

rav::RavennaBrowser::RavennaBrowser(boost::asio::io_context& io_context) : io_context_(io_context) {}

const rav::dnssd::ServiceDescription* rav::RavennaBrowser::find_session(const std::string& session_name) const {
    if (session_browser_ == nullptr) {
        return nullptr;
    }
    return session_browser_->find_service(session_name);
}

const rav::dnssd::ServiceDescription* rav::RavennaBrowser::find_node(const std::string& node_name) const {
    if (node_browser_ == nullptr) {
        return nullptr;
    }
    return node_browser_->find_service(node_name);
}

bool rav::RavennaBrowser::subscribe(Subscriber* subscriber_to_add) {
    if (subscribers_.add(subscriber_to_add)) {
        if (node_browser_) {
            for (auto& s : node_browser_->get_services()) {
                if (s.host_target.empty()) {
                    continue;  // Only consider resolved services
                }
                subscriber_to_add->ravenna_node_discovered({s});
            }
        }

        if (session_browser_) {
            for (auto& s : session_browser_->get_services()) {
                if (s.host_target.empty()) {
                    continue;  // Only consider resolved services
                }
                subscriber_to_add->ravenna_session_discovered({s});
            }
        }
        return true;
    }
    return false;
}

bool rav::RavennaBrowser::unsubscribe(const Subscriber* subscriber_to_remove) {
    return subscribers_.remove(subscriber_to_remove);
}

tl::expected<void, const char*> rav::RavennaBrowser::set_node_browsing_enabled(const bool enabled) {
    if (enabled == (node_browser_ != nullptr)) {
        return {};  // Nothing to do here
    }

    if (enabled) {
        node_browser_ = dnssd::Browser::create(io_context_);

        if (node_browser_ == nullptr) {
            return tl::unexpected("No dnssd browser available");
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

        node_browser_->browse_for("_rtsp._tcp,_ravenna");
    } else {
        if (node_browser_ == nullptr) {
            return {};
        }

        for (auto& desc : node_browser_->get_services()) {
            if (desc.host_target.empty()) {
                continue;  // Only consider resolved services
            }
            for (auto* s : subscribers_) {
                s->ravenna_node_removed(desc);
            }
        }

        node_browser_.reset();
    }

    return {};
}

tl::expected<void, const char*> rav::RavennaBrowser::set_session_browsing_enabled(const bool enabled) {
    if (enabled == (session_browser_ != nullptr)) {
        return {};  // No change
    }

    if (enabled) {
        session_browser_ = dnssd::Browser::create(io_context_);

        if (session_browser_ == nullptr) {
            return tl::unexpected("No dnssd browser available");
        }

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

        session_browser_->browse_for("_rtsp._tcp,_ravenna_session");
    } else {
        if (session_browser_ == nullptr) {
            return {};
        }

        for (auto& desc : session_browser_->get_services()) {
            if (desc.host_target.empty()) {
                continue;  // Only consider resolved services
            }
            for (auto* s : subscribers_) {
                s->ravenna_session_removed(desc);
            }
        }

        session_browser_.reset();
    }

    return {};
}
