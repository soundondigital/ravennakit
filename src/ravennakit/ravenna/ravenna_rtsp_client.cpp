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

rav::ravenna_rtsp_client::ravenna_rtsp_client(asio::io_context& io_context, dnssd::dnssd_browser& browser) :
    io_context_(io_context), browser_(browser) {
    browser_subscriber_->on<dnssd::dnssd_browser::service_resolved>([this](const auto& event) {
        RAV_INFO("RAVENNA Stream resolved: {}", event.description.name);
        for (auto& session : sessions_) {
            if (event.description.name == session.session_name) {
                session.host_target = event.description.host_target;
                session.port = event.description.port;
                auto& connection = find_or_create_connection(session.host_target, session.port);
                connection.client.async_describe(fmt::format("/by-name/{}", session.session_name));
            }
        }
    });
    browser.subscribe(browser_subscriber_);
}

void rav::ravenna_rtsp_client::subscribe(const std::string& session_name, subscriber& s) {
    if (session_name.empty()) {
        RAV_THROW_EXCEPTION("session_name cannot be empty");
    }

    // Subscribe to existing session
    for (auto& session : sessions_) {
        if (session.session_name == session_name) {
            session.subscribers.push_back(s);
            if (session.sdp_.has_value()) {
                s->emit(announced {session_name, *session.sdp_});
            }
            return;
        }
    }

    // Create new session
    auto& new_session = sessions_.emplace_back();
    new_session.session_name = session_name;
    new_session.subscribers.push_back(s);

    if (auto* service = browser_.find_service(session_name)) {
        new_session.host_target = service->host_target;
        new_session.port = service->port;

        auto& connection = find_or_create_connection(service->host_target, service->port);
        connection.client.async_describe(fmt::format("/by-name/{}", session_name));
    }
}

rav::ravenna_rtsp_client::connection_context&
rav::ravenna_rtsp_client::find_or_create_connection(const std::string& host_target, const uint16_t port) {
    for (auto& connection : connections_) {
        if (connection.host_target == host_target && connection.port == port) {
            return connection;
        }
    }
    connections_.push_back({host_target, port, rtsp_client {io_context_}});
    auto& new_connection = connections_.back();

    new_connection.client.on<rtsp_connect_event>([=](const auto&) {
        RAV_TRACE("RTSP Client connected for host target: {}", host_target);
    });
    new_connection.client.on<rtsp_response>([=](const auto&) {
        RAV_TRACE("RTSP Client response for host target: {}", host_target);
    });
    new_connection.client.on<rtsp_request>([=](const auto&) {
        RAV_TRACE("RTSP Client request for host target: {}", host_target);
    });
    new_connection.client.async_connect(host_target, port);
    RAV_TRACE("RTSP Created RTSP connection to {}:{}", host_target, port);
    return new_connection;
}
