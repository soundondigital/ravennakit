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
    // auto& it = sessions_.emplace_back();
    // it.session_name = session_name;
    // it.subscribers.push_back(s);
    //
    // if (auto* service = browser_.find_service(session_name)) {
    //     connect_to_service(*service);
    // } else {
    //
    // }
}

void rav::ravenna_rtsp_client::describe_session(const std::string& session_name) {
    // Try to find the service for the session name
    const auto* service = browser_.find_service(session_name);
    if (service == nullptr) {
        RAV_TRACE("No service available for: {}", session_name);
        return;
    }

    // auto host_target = service->host_target;
    // auto connection = connections_.find(host_target);
    // if (connection == connections_.end()) {
    //     connection = connections_.insert({host_target, rtsp_client(io_context_)}).first;
    //     connection->second.on<rtsp_connect_event>([this, host_target](const auto&) {
    //         RAV_TRACE("RTSP Client connected for host target: {}", host_target);
    //     });
    //     connection->second.on<rtsp_response>([host_target](const auto&) {
    //         RAV_TRACE("RTSP Client response for host target: {}", host_target);
    //     });
    //     connection->second.on<rtsp_request>([host_target](const auto&) {
    //         RAV_TRACE("RTSP Client request for host target: {}", host_target);
    //     });
    //     connection->second.connect(service->host_target, service->port);
    // }

    // connection->second.describe(fmt::format("/by-name/{}", session_name));
}

void rav::ravenna_rtsp_client::connect_to_service(const dnssd::service_description& service) {
    // auto connection = connections_.find(service.fullname);
    // if (connection == connections_.end()) {}

    // for (auto& [session_name, session_context] : sessions_) {
    //     if (service.name == session_name) {
    //         if (!session_context.sdp_.has_value()) {
    //             describe_session(session_name);
    //         }
    //     }
    // }
    //
    // for (auto& [host_target, connection] : connections_) {
    //     if (service.host_target == host_target) {
    //
    //     }
    // }
}

rav::rtsp_client* rav::ravenna_rtsp_client::get_or_create_service_connection(const dnssd::service_description& service
) {
    return nullptr;
}
