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
    unsubscribe_from_ravenna_rtsp_client();
}

void rav::ravenna_rtsp_client::subscriber::subscribe_to_ravenna_rtsp_client(ravenna_rtsp_client& client, const std::string& session_name) {
    if (session_name.empty()) {
        RAV_THROW_EXCEPTION("session_name cannot be empty");
    }

    unsubscribe_from_ravenna_rtsp_client();

    node_ = {this, &client};

    // Subscribe to existing session
    for (auto& session : client.sessions_) {
        if (session.session_name == session_name) {
            session.subscribers.push_back(node_);
            if (session.sdp_.has_value()) {
                on_announced(announced_event {session_name, *session.sdp_});
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

void rav::ravenna_rtsp_client::subscriber::unsubscribe_from_ravenna_rtsp_client() {
    node_.unlink();
    if (auto* owner = node_->second) {
        owner->do_maintenance();
    }
    node_.reset_value();
}

rav::ravenna_rtsp_client::ravenna_rtsp_client(asio::io_context& io_context, dnssd::dnssd_browser& browser) :
    io_context_(io_context), browser_(browser) {
    browser_subscriber_->on<dnssd::dnssd_browser::service_resolved>([this](const auto& event) {
        RAV_TRACE("RAVENNA Stream resolved: {}", event.description.name);
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

    new_connection.client.on<rtsp_connection::connect_event>([=](const auto&) {
        RAV_TRACE("Connected to: rtsp://{}:{}", host_target, port);
    });
    new_connection.client.on<rtsp_connection::request_event>([this, &client = new_connection.client](const auto& event) {
        RAV_TRACE("{}", event.request.to_debug_string(true));

        if (event.request.method == "ANNOUNCE") {
            if (auto* content_type = event.request.headers.get("content-type")) {
                if (!rav::string_starts_with(content_type->value, "application/sdp")) {
                    RAV_ERROR("RTSP request has unexpected Content-Type: {}", content_type->value);
                    return;
                }
            }
            // Note: the content-type header is not always present, at least for some devices.
            handle_incoming_sdp(event.request.data);
            return;
        }

        if (event.request.method == "GET_PARAMETER") {
            if (event.request.data.empty()) {
                // Interpret as liveliness check (ping) (https://datatracker.ietf.org/doc/html/rfc2326#section-10.8)
                rtsp_response response;
                response.status_code = 200;
                response.reason_phrase = "OK";
                client.async_send_response(response);
            } else {
                RAV_WARNING("Unsupported parameter: {}", event.request.uri);
            }
            return;
        }

        RAV_WARNING("Unhandled RTSP request: {}", event.request.method);
    });
    new_connection.client.on<rtsp_connection::response_event>([=](const auto& event) {
        RAV_TRACE("{}", event.response.to_debug_string(true));

        if (event.response.status_code != 200) {
            RAV_ERROR("RTSP request failed with status: {} {}", event.response.status_code, event.response.reason_phrase);
            return;
        }

        if (auto* content_type = event.response.headers.get("content-type")) {
            if (!rav::string_starts_with(content_type->value, "application/sdp")) {
                RAV_ERROR("RTSP response has unexpected Content-Type: {}", content_type->value);
                return;
            }
            handle_incoming_sdp(event.response.data);
            return;
        }

        RAV_ERROR("RTSP response missing Content-Type header");
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

    const auto& connection = find_or_create_connection(service.host_target, service.port);
    connection.client.async_describe(fmt::format("/by-name/{}", session.session_name));
}

void rav::ravenna_rtsp_client::do_maintenance() {
    for (auto& session : sessions_) {
        if (!session.subscribers.is_linked()) {
            if (!session.host_target.empty() && session.port != 0) {
                if (const auto* connection = find_connection(session.host_target, session.port)) {
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

void rav::ravenna_rtsp_client::handle_incoming_sdp(const std::string& sdp_text) {
    auto result = sdp::session_description::parse_new(sdp_text);
    if (result.is_err()) {
        RAV_ERROR("Failed to parse SDP: {}", result.get_err());
        return;
    }

    auto sdp = result.move_ok();

    for (auto& session : sessions_) {
        if (session.session_name == sdp.session_name()) {
            session.sdp_ = sdp;
            session.subscribers.foreach ([&](auto& node) {
                if (auto* s = node->first) {
                    s->on_announced(announced_event {session.session_name, sdp});
                }
            });
        }
    }
}
