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

rav::RavennaRtspClient::RavennaRtspClient(asio::io_context& io_context, RavennaBrowser& browser) :
    io_context_(io_context), browser_(browser) {
    if (!browser_.subscribe(this)) {
        RAV_WARNING("Failed to add subscriber to browser");
    }
}

rav::RavennaRtspClient::~RavennaRtspClient() {
    if (!browser_.unsubscribe(this)) {
        RAV_WARNING("Failed to remove subscriber from browser");
    }
}

bool rav::RavennaRtspClient::subscribe_to_session(Subscriber* subscriber_to_add, const std::string& session_name) {
    RAV_ASSERT(subscriber_to_add != nullptr, "Subscriber must not be nullptr");

    // Subscribe to existing session
    for (auto& session : sessions_) {
        if (session.session_name == session_name) {
            if (!session.subscribers.add(subscriber_to_add)) {
                RAV_WARNING("Failed to add subscriber");
                return false;
            }
            if (session.sdp_.has_value()) {
                subscriber_to_add->on_announced(AnnouncedEvent {session_name, *session.sdp_});
            }
            return true;
        }
    }

    // Create new session
    auto& new_session = sessions_.emplace_back();
    new_session.session_name = session_name;
    if (!new_session.subscribers.add(subscriber_to_add)) {
        RAV_WARNING("Failed to add subscriber");
        sessions_.pop_back();  // Roll back
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

bool rav::RavennaRtspClient::unsubscribe_from_all_sessions(const Subscriber* subscriber_to_remove) {
    RAV_ASSERT(subscriber_to_remove != nullptr, "Subscriber must not be nullptr");

    auto count = 0;
    for (auto& session : sessions_) {
        if (session.subscribers.remove(subscriber_to_remove)) {
            count++;
        }
    }
    do_maintenance();
    return count > 0;
}

void rav::RavennaRtspClient::ravenna_session_discovered(const dnssd::Browser::ServiceResolved& event) {
    RAV_TRACE("RAVENNA session resolved: {}", event.description.to_string());
    for (auto& session : sessions_) {
        if (event.description.name == session.session_name) {
            update_session_with_service(session, event.description);
        }
    }
}

std::optional<rav::sdp::SessionDescription>
rav::RavennaRtspClient::get_sdp_for_session(const std::string& session_name) const {
    for (auto& session : sessions_) {
        if (session.session_name == session_name) {
            return session.sdp_;
        }
    }
    return std::nullopt;
}

std::optional<std::string> rav::RavennaRtspClient::get_sdp_text_for_session(const std::string& session_name) const {
    for (auto& session : sessions_) {
        if (session.session_name == session_name) {
            return session.sdp_text_;
        }
    }
    return std::nullopt;
}

asio::io_context& rav::RavennaRtspClient::get_io_context() const {
    return io_context_;
}

rav::RavennaRtspClient::ConnectionContext&
rav::RavennaRtspClient::find_or_create_connection(const std::string& host_target, const uint16_t port) {
    if (const auto connection = find_connection(host_target, port)) {
        return *connection;
    }

    connections_.push_back(
        std::make_unique<ConnectionContext>(ConnectionContext {host_target, port, rtsp::Client {io_context_}})
    );
    const auto& new_connection = connections_.back();

    new_connection->client.on<rtsp::Connection::ConnectEvent>([=](const auto&) {
        RAV_TRACE("Connected to: rtsp://{}:{}", host_target, port);
    });
    new_connection->client.on<rtsp::Connection::RequestEvent>([this,
                                                               &client = new_connection->client](const auto& event) {
        RAV_TRACE("{}", event.rtsp_request.to_debug_string(true));

        if (event.rtsp_request.method == "ANNOUNCE") {
            if (auto* content_type = event.rtsp_request.rtsp_headers.get("content-type")) {
                if (!rav::string_starts_with(content_type->value, "application/sdp")) {
                    RAV_ERROR("RTSP request has unexpected Content-Type: {}", content_type->value);
                    return;
                }
            }
            // Note: the content-type header is not always present, at least for some devices.
            handle_incoming_sdp(event.rtsp_request.data);
            return;
        }

        if (event.rtsp_request.method == "GET_PARAMETER") {
            if (event.rtsp_request.data.empty()) {
                // Interpret as liveliness check (ping) (https://datatracker.ietf.org/doc/html/rfc2326#section-10.8)
                rtsp::Response response;
                response.status_code = 200;
                response.reason_phrase = "OK";
                client.async_send_response(response);
            } else {
                RAV_WARNING("Unsupported parameter: {}", event.rtsp_request.uri);
            }
            return;
        }

        RAV_WARNING("Unhandled RTSP request: {}", event.rtsp_request.method);
    });
    new_connection->client.on<rtsp::Connection::ResponseEvent>([=](const auto& event) {
        RAV_TRACE("{}", event.rtsp_response.to_debug_string(true));

        if (event.rtsp_response.status_code != 200) {
            RAV_ERROR(
                "RTSP request failed with status: {} {}", event.rtsp_response.status_code,
                event.rtsp_response.reason_phrase
            );
            return;
        }

        if (!event.rtsp_response.data.empty()) {
            if (auto* content_type = event.rtsp_response.rtsp_headers.get("content-type")) {
                if (!rav::string_starts_with(content_type->value, "application/sdp")) {
                    RAV_ERROR("RTSP response has unexpected Content-Type: {}", content_type->value);
                    return;
                }
                handle_incoming_sdp(event.rtsp_response.data);
                return;
            }

            RAV_ERROR("RTSP response missing Content-Type header");
        }
    });
    new_connection->client.async_connect(host_target, port);
    return *new_connection;
}

rav::RavennaRtspClient::ConnectionContext*
rav::RavennaRtspClient::find_connection(const std::string& host_target, const uint16_t port) const {
    for (const auto& connection : connections_) {
        if (connection->host_target == host_target && connection->port == port) {
            return connection.get();
        }
    }
    return nullptr;
}

void rav::RavennaRtspClient::update_session_with_service(
    SessionContext& session, const dnssd::ServiceDescription& service
) {
    session.host_target = string_remove_suffix(service.host_target, ".");
    session.port = service.port;

    auto& connection = find_or_create_connection(session.host_target, session.port);
    connection.client.async_describe(fmt::format("/by-name/{}", session.session_name));
}

void rav::RavennaRtspClient::do_maintenance() {
    for (auto& session : sessions_) {
        if (session.subscribers.empty()) {
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
                return session.subscribers.empty();
            }
        ),
        sessions_.end()
    );
}

void rav::RavennaRtspClient::handle_incoming_sdp(const std::string& sdp_text) {
    auto result = sdp::SessionDescription::parse_new(sdp_text);
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
                s->on_announced(AnnouncedEvent {session.session_name, sdp});
            });
        }
    }
}
