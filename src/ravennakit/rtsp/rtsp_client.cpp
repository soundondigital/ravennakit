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
#include "ravennakit/rtsp/rtsp_request.hpp"
#include "ravennakit/util/uri.hpp"

rav::rtsp_client::rtsp_client(asio::io_context& io_context) :
    rtsp_connection(asio::ip::tcp::socket(io_context)), resolver_(io_context) {}

void rav::rtsp_client::async_connect(const std::string& host, const uint16_t port) {
    async_resolve_connect(host, std::to_string(port), asio::ip::resolver_base::flags::numeric_service);
}

void rav::rtsp_client::async_connect(const std::string& host, const std::string& service) {
    async_resolve_connect(host, service, asio::ip::resolver_base::flags());
}

void rav::rtsp_client::async_describe(const std::string& path, std::string data) {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    rtsp_request request;
    request.method = "DESCRIBE";
    request.uri = uri::encode("rtsp", host_, path);
    request.headers["CSeq"] = "15";
    request.headers["Accept"] = "application/sdp";
    request.data = std::move(data);

    async_send_request(request);
}

void rav::rtsp_client::async_setup(const std::string& path) {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    rtsp_request request;
    request.method = "SETUP";
    request.uri = uri::encode("rtsp", host_, path);
    request.headers["CSeq"] = "15";
    request.headers["Transport"] = "RTP/AVP;unicast;client_port=5004-5005";

    async_send_request(request);
}

void rav::rtsp_client::async_play(const std::string& path) {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    rtsp_request request;
    request.method = "PLAY";
    request.uri = uri::encode("rtsp", host_, path);
    request.headers["CSeq"] = "15";
    request.headers["Transport"] = "RTP/AVP;unicast;client_port=5004-5005";

    async_send_request(request);
}

void rav::rtsp_client::async_teardown(const std::string& path) {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    rtsp_request request;
    request.method = "TEARDOWN";
    request.uri = uri::encode("rtsp", host_, path);
    request.headers["CSeq"] = "15";

    async_send_request(request);
}

void rav::rtsp_client::on_connected() {
    emit(rtsp_connect_event {*this});
}

void rav::rtsp_client::on_rtsp_request(const rtsp_request& request) {
    emit(request);
}

void rav::rtsp_client::on_rtsp_response(const rtsp_response& response) {
    emit(response);
}

void rav::rtsp_client::async_resolve_connect(
    const std::string& host, const std::string& service, const asio::ip::resolver_base::flags flags
) {
    host_ = host;
    resolver_.async_resolve(
        host, service, flags,
        [this, host](const asio::error_code resolve_error, const asio::ip::tcp::resolver::results_type& results) {
            if (resolve_error) {
                RAV_ERROR("Resolve error: {}", resolve_error.message());
                return;
            }

            if (results.empty()) {
                RAV_ERROR("No results found for host: {}", host);
                return;
            }

            for (auto& result : results) {
                RAV_TRACE("Resolved: {} for host \"{}\"", result.endpoint().address().to_string(), host);
            }

            rtsp_connection::async_connect(results);
        }
    );
}
