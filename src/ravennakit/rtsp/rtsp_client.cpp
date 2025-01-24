/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/rtsp_client.hpp"

#include "ravennakit/core/log.hpp"
#include "ravennakit/rtsp/detail/rtsp_request.hpp"
#include "ravennakit/core/uri.hpp"

rav::rtsp_client::rtsp_client(asio::io_context& io_context) :
    resolver_(io_context), connection_(rtsp_connection::create(asio::ip::tcp::socket(io_context))) {}

rav::rtsp_client::~rtsp_client() {
    if (connection_ != nullptr) {
        connection_->set_subscriber(nullptr);
    }
}

void rav::rtsp_client::async_connect(const std::string& host, const uint16_t port) {
    async_resolve_connect(host, std::to_string(port), asio::ip::resolver_base::flags::numeric_service);
}

void rav::rtsp_client::async_connect(const std::string& host, const std::string& service) {
    async_resolve_connect(host, service, asio::ip::resolver_base::flags());
}

void rav::rtsp_client::async_describe(const std::string& path, std::string data) const {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    rtsp_request request;
    request.method = "DESCRIBE";
    request.uri = uri::encode("rtsp", host_, path);
    request.headers.set("CSeq", "15");
    request.headers.set("Accept", "application/sdp");
    request.data = std::move(data);

    connection_->async_send_request(request);
}

void rav::rtsp_client::async_setup(const std::string& path) const {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    rtsp_request request;
    request.method = "SETUP";
    request.uri = uri::encode("rtsp", host_, path);
    request.headers.set("CSeq", "15");
    request.headers.set("Transport", "RTP/AVP;unicast;client_port=5004-5005");

    connection_->async_send_request(request);
}

void rav::rtsp_client::async_play(const std::string& path) const {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    rtsp_request request;
    request.method = "PLAY";
    request.uri = uri::encode("rtsp", host_, path);
    request.headers.set("CSeq", "15");
    request.headers.set("Transport", "RTP/AVP;unicast;client_port=5004-5005");

    connection_->async_send_request(request);
}

void rav::rtsp_client::async_teardown(const std::string& path) const {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    rtsp_request request;
    request.method = "TEARDOWN";
    request.uri = uri::encode("rtsp", host_, path);
    request.headers.set("CSeq", "15");

    connection_->async_send_request(request);
}

void rav::rtsp_client::async_send_response(const rtsp_response& response) const {
    connection_->async_send_response(response);
}

void rav::rtsp_client::async_send_request(const rtsp_request& request) const {
    connection_->async_send_request(request);
}

void rav::rtsp_client::on_connect(rtsp_connection& connection) {
    events_.emit(rtsp_connection::connect_event {connection});
}

void rav::rtsp_client::on_request(rtsp_connection& connection, const rtsp_request& request) {
    events_.emit(rtsp_connection::request_event {connection, request});
}

void rav::rtsp_client::on_response(rtsp_connection& connection, const rtsp_response& response) {
    events_.emit(rtsp_connection::response_event {connection, response});
}

void rav::rtsp_client::async_resolve_connect(
    const std::string& host, const std::string& service, const asio::ip::resolver_base::flags flags
) {
    host_ = host;
    connection_->set_subscriber(this);
    auto connection = connection_;
    resolver_.async_resolve(
        host, service, flags,
        [host, connection](const asio::error_code ec, const asio::ip::tcp::resolver::results_type& results) {
            if (ec) {
                RAV_ERROR("Resolve error: {}", ec.message());
                return;
            }

            if (results.empty()) {
                RAV_ERROR("No results found for host: {}", host);
                return;
            }

            for (auto& result : results) {
                RAV_TRACE("Resolved: {} for host \"{}\"", result.endpoint().address().to_string(), host);
            }

            connection->async_connect(results);
        }
    );
}
