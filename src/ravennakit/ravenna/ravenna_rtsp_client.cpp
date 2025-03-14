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

rav::ravenna_rtsp_client::ravenna_rtsp_client(asio::io_context& io_context, ravenna_browser& browser) :
    io_context_(io_context), browser_(browser) {
    if (!browser_.subscribe(this)) {
        RAV_WARNING("Failed to add subscriber to browser");
    }
}

rav::ravenna_rtsp_client::~ravenna_rtsp_client() {
    if (!browser_.unsubscribe(this)) {
        RAV_WARNING("Failed to remove subscriber from browser");
    }
}

bool rav::ravenna_rtsp_client::subscribe_to_session(subscriber* subscriber, const std::string& session_name) {
    RAV_ASSERT(subscriber != nullptr, "Subscriber must not be nullptr");

    // Subscribe to existing session
    for (auto& session : sessions_) {
        if (session.session_name == session_name) {
            if (!session.subscribers.add(subscriber)) {
                RAV_WARNING("Failed to add subscriber");
                return false;
            }
            if (session.sdp_.has_value()) {
                subscriber->on_announced(announced_event {session_name, *session.sdp_});
            }
            return true;
        }
    }

    // Create new session
    auto& new_session = sessions_.emplace_back();
    new_session.session_name = session_name;
    if (!new_session.subscribers.add(subscriber)) {
        RAV_WARNING("Failed to add subscriber");
        sessions_.pop_back(); // Roll back
        return false;
    }

    // Get things going for if a session is already available
    if (auto* service = browser_.find_session(session_name)) {
        if (service->resolved()) {
            update_session_with_service(new_session, *service);
        }
    }

    return true;
}

bool rav::ravenna_rtsp_client::unsubscribe_from_all_sessions(subscriber* subscriber) {
    RAV_ASSERT(subscriber != nullptr, "Subscriber must not be nullptr");

    auto count = 0;
    for (auto& session : sessions_) {
        if (session.subscribers.remove(subscriber)) {
            count++;
        }
    }
    do_maintenance();
    return count > 0;
}

void rav::ravenna_rtsp_client::ravenna_session_discovered(const dnssd::dnssd_browser::service_resolved& event) {
    RAV_TRACE("RAVENNA session resolved: {}", event.description.to_string());
    for (auto& session : sessions_) {
        if (event.description.name == session.session_name) {
            update_session_with_service(session, event.description);
        }
    }
}

std::optional<rav::sdp::session_description>
rav::ravenna_rtsp_client::get_sdp_for_session(const std::string& session_name) const {
    for (auto& session : sessions_) {
        if (session.session_name == session_name) {
            return session.sdp_;
        }
    }
    return std::nullopt;
}

std::optional<std::string> rav::ravenna_rtsp_client::get_sdp_text_for_session(const std::string& session_name) const {
    for (auto& session : sessions_) {
        if (session.session_name == session_name) {
            return session.sdp_text_;
        }
    }
    return std::nullopt;
}

asio::io_context& rav::ravenna_rtsp_client::get_io_context() const {
    return io_context_;
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
    new_connection.client.on<rtsp_connection::request_event>([this,
                                                              &client = new_connection.client](const auto& event) {
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
            RAV_ERROR(
                "RTSP request failed with status: {} {}", event.response.status_code, event.response.reason_phrase
            );
            return;
        }

        if (!event.response.data.empty()) {
            if (auto* content_type = event.response.headers.get("content-type")) {
                if (!rav::string_starts_with(content_type->value, "application/sdp")) {
                    RAV_ERROR("RTSP response has unexpected Content-Type: {}", content_type->value);
                    return;
                }
                handle_incoming_sdp(event.response.data);
                return;
            }

            RAV_ERROR("RTSP response missing Content-Type header");
        }
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
        if (session.subscribers.empty()) {
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
                return session.subscribers.empty();
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
            session.sdp_text_ = sdp_text;

            session.subscribers.foreach ([&](auto s) {
                s->on_announced(announced_event {session.session_name, sdp});
            });
        }
    }
}
