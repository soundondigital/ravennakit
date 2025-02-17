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

    session_browser_subscriber_->on<dnssd::dnssd_browser::service_resolved>([this](const auto& event) {
        for (auto& s : subscribers_) {
            s->ravenna_session_discovered(event);
        }
    });
    session_browser_->subscribe(session_browser_subscriber_);

    node_browser_->browse_for("_rtsp._tcp,_ravenna");
    session_browser_->browse_for("_rtsp._tcp,_ravenna_session");

    maintenance_thread_ = std::thread([this] {
        io_context_.run();
    });
}

rav::ravenna_node::~ravenna_node() {
    io_context_.stop();
    if (maintenance_thread_.joinable()) {
        maintenance_thread_.join();
    }
}

void rav::ravenna_node::create_receiver(const std::string& ravenna_session_name) {
    std::ignore = ravenna_session_name;
}

bool rav::ravenna_node::subscribe(subscriber* s) {
    std::promise<bool> promise;
    auto future = promise.get_future();

    asio::dispatch(io_context_, [this, s, p = std::move(promise)]() mutable {
        for (auto& service : session_browser_->get_services()) {
            s->ravenna_session_discovered(dnssd::dnssd_browser::service_resolved {service});
        }
        p.set_value(subscribers_.add(s));
    });

    return future.get();
}

bool rav::ravenna_node::unsubscribe(subscriber* s) {
    std::promise<bool> promise;
    auto future = promise.get_future();

    asio::dispatch(io_context_, [this, s, p = std::move(promise)]() mutable {
        p.set_value(subscribers_.remove(s));
    });

    return future.get();
}
