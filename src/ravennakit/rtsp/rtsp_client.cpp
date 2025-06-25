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
#include "ravennakit/core/util/uri.hpp"

rav::rtsp::Client::Client(boost::asio::io_context& io_context) :
    resolver_(io_context), connection_(Connection::create(boost::asio::ip::tcp::socket(io_context))) {}

rav::rtsp::Client::~Client() {
    if (connection_ != nullptr) {
        connection_->set_subscriber(nullptr);
    }
}

void rav::rtsp::Client::async_connect(const std::string& host, const uint16_t port) {
    async_resolve_connect(host, std::to_string(port), boost::asio::ip::resolver_base::flags::numeric_service);
}

void rav::rtsp::Client::async_connect(const std::string& host, const std::string& service) {
    async_resolve_connect(host, service, boost::asio::ip::resolver_base::flags());
}

void rav::rtsp::Client::async_describe(const std::string& path, std::string data) {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    Request request;
    request.method = "DESCRIBE";
    request.uri = Uri::encode("rtsp", host_, path);
    request.rtsp_headers.set("CSeq", fmt::format("{}", seq_++));
    request.rtsp_headers.set("Accept", "application/sdp");
    request.data = std::move(data);

    connection_->async_send_request(request);
}

void rav::rtsp::Client::async_setup(const std::string& path) {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    Request request;
    request.method = "SETUP";
    request.uri = Uri::encode("rtsp", host_, path);
    request.rtsp_headers.set("CSeq", fmt::format("{}", seq_++));
    request.rtsp_headers.set("Transport", "RTP/AVP;unicast;client_port=5004-5005");

    connection_->async_send_request(request);
}

void rav::rtsp::Client::async_play(const std::string& path) {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    Request request;
    request.method = "PLAY";
    request.uri = Uri::encode("rtsp", host_, path);
    request.rtsp_headers.set("CSeq", fmt::format("{}", seq_++));
    request.rtsp_headers.set("Transport", "RTP/AVP;unicast;client_port=5004-5005");

    connection_->async_send_request(request);
}

void rav::rtsp::Client::async_teardown(const std::string& path) {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    Request request;
    request.method = "TEARDOWN";
    request.uri = Uri::encode("rtsp", host_, path);
    request.rtsp_headers.set("CSeq", fmt::format("{}", seq_++));

    connection_->async_send_request(request);
}

void rav::rtsp::Client::async_send_response(const Response& response) const {
    connection_->async_send_response(response);
}

void rav::rtsp::Client::async_send_request(const Request& request) const {
    connection_->async_send_request(request);
}

void rav::rtsp::Client::on_connect(Connection& connection) {
    events_.emit(Connection::ConnectEvent {connection});
}

void rav::rtsp::Client::on_request(Connection& connection, const Request& request) {
    events_.emit(Connection::RequestEvent {connection, request});
}

void rav::rtsp::Client::on_response(Connection& connection, const Response& response) {
    events_.emit(Connection::ResponseEvent {connection, response});
}

void rav::rtsp::Client::async_resolve_connect(
    const std::string& host, const std::string& service, const boost::asio::ip::resolver_base::flags flags
) {
    host_ = host;
    connection_->set_subscriber(this);
    auto connection = connection_;
    resolver_.async_resolve(
        host, service, flags,
        [host, connection](const boost::system::error_code ec, const boost::asio::ip::tcp::resolver::results_type& results) {
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
