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

rav::ravenna_rtsp_client::subscriber::~subscriber() {
    unsubscribe();
}

void rav::ravenna_rtsp_client::subscriber::subscribe(ravenna_rtsp_client& client, const std::string& session_name) {
    if (session_name.empty()) {
        RAV_THROW_EXCEPTION("session_name cannot be empty");
    }

    unsubscribe();

    node_ = {this, &client};

    // Subscribe to existing session
    for (auto& session : client.sessions_) {
        if (session.session_name == session_name) {
            session.subscribers.push_back(node_);
            if (session.sdp_.has_value()) {
                emit(announced {session_name, *session.sdp_});
            }
            return;
        }
    }

    // Create new session
    auto& new_session = client.sessions_.emplace_back();
    new_session.session_name = session_name;
    new_session.subscribers.push_back(node_);

    // Get things going if a service is already available
    if (auto* service = client.browser_.find_service(session_name)) {
        client.update_session_with_service(new_session, *service);
    }
}

void rav::ravenna_rtsp_client::subscriber::unsubscribe() {
    node_.unlink();
    if (auto* owner = node_.value().second) {
        owner->schedule_maintenance();
    }
    node_.reset();
}

rav::ravenna_rtsp_client::ravenna_rtsp_client(asio::io_context& io_context, dnssd::dnssd_browser& browser) :
    io_context_(io_context), browser_(browser) {
    browser_subscriber_->on<dnssd::dnssd_browser::service_resolved>([this](const auto& event) {
        RAV_INFO("RAVENNA Stream resolved: {}", event.description.name);
        for (auto& session : sessions_) {
            if (event.description.name == session.session_name) {
                update_session_with_service(session, event.description);
            }
        }
    });
    browser.subscribe(browser_subscriber_);
}

rav::ravenna_rtsp_client::~ravenna_rtsp_client() {
    for (auto& session : sessions_) {
        session.subscribers.foreach ([](auto& node) {
            node.reset();
        });
    }
}

rav::ravenna_rtsp_client::connection_context&
rav::ravenna_rtsp_client::find_or_create_connection(const std::string& host_target, const uint16_t port) {
    if (const auto connection = find_connection(host_target, port)) {
        return *connection;
    }

    connections_.push_back({host_target, port, rtsp_client {io_context_}});
    auto& new_connection = connections_.back();

    new_connection.client.on<rtsp_connect_event>([=](const auto&) {
        RAV_TRACE("Connected to: rtsp://{}:{}", host_target, port);
    });
    new_connection.client.on<rtsp_request>([=](const auto& request) {
        RAV_INFO("{}\n{}", request.to_debug_string(), rav::string_replace(request.data, "\r\n", "\n"));
    });
    new_connection.client.on<rtsp_response>([=](const auto& response) {
        RAV_INFO("{}\n{}", response.to_debug_string(), rav::string_replace(response.data, "\r\n", "\n"));
    });
    new_connection.client.async_connect(host_target, port);
    return new_connection;
}

rav::ravenna_rtsp_client::connection_context*
rav::ravenna_rtsp_client::find_connection(const std::string& host_target, const uint16_t port) {
    for (auto& connection : connections_) {
        if (connection.host_target == host_target && connection.port == port) {
            return &connection;
        }
    }
    return nullptr;
}

void rav::ravenna_rtsp_client::update_session_with_service(
    session_context& session, const dnssd::service_description& service
) {
    session.host_target = service.host_target;
    session.port = service.port;

    auto& connection = find_or_create_connection(service.host_target, service.port);
    connection.client.async_describe(fmt::format("/by-name/{}", session.session_name));
}

void rav::ravenna_rtsp_client::schedule_maintenance() {
    for (auto& session : sessions_) {
        if (!session.subscribers.is_linked()) {
            if (!session.host_target.empty() && session.port != 0) {
                if (auto* connection = find_connection(session.host_target, session.port)) {
                    connection->client.async_teardown(fmt::format("/by-name/{}", session.session_name));
                }
            }
        }
    }

    sessions_.erase(
        std::remove_if(
            sessions_.begin(), sessions_.end(),
            [](const auto& session) {
                return !session.subscribers.is_linked();
            }
        ),
        sessions_.end()
    );
}
