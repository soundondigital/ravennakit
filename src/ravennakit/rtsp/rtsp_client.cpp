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

rav::rtsp_client::rtsp_client(asio::io_context& io_context) : resolver_(io_context), socket_(io_context) {}

void rav::rtsp_client::async_connect(const std::string& host, const uint16_t port) {
    async_connect(host, std::to_string(port), asio::ip::resolver_base::flags::numeric_service);
}

void rav::rtsp_client::async_connect(const std::string& host, const std::string& service) {
    async_connect(host, service, asio::ip::resolver_base::flags());
}

void rav::rtsp_client::async_describe(const std::string& path) {
    if (!string_starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    rtsp_request request;
    request.method = "DESCRIBE";
    request.uri = uri::encode("rtsp", host_, path);
    request.headers["CSeq"] = "15";
    request.headers["Accept"] = "application/sdp";

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

void rav::rtsp_client::async_send_response(const rtsp_response& response) {
    const auto encoded = response.encode();
    RAV_TRACE("Sending response: {}", response.to_debug_string(false));
    const bool should_trigger_async_write = output_buffer_.exhausted() && socket_.is_open();
    output_buffer_.write(encoded);
    if (should_trigger_async_write) {
        async_write();
    }
}

void rav::rtsp_client::async_send_request(const rtsp_request& request) {
    const auto encoded = request.encode();
    RAV_TRACE("Sending request: {}", request.to_debug_string(false));
    const bool should_trigger_async_write = output_buffer_.exhausted() && socket_.is_open();
    output_buffer_.write(encoded);
    if (should_trigger_async_write) {
        async_write();
    }
}

void rav::rtsp_client::post(std::function<void()> work) {
    asio::post(socket_.get_executor(), std::move(work));
}

void rav::rtsp_client::async_connect(
    const std::string& host, const std::string& service, const asio::ip::resolver_base::flags flags
) {
    if (!parser_.has_handler<rtsp_request>()) {
        parser_.on<rtsp_request>([this](const rtsp_request& request) {
            emit(request);
        });
    }

    if (!parser_.has_handler<rtsp_response>()) {
        parser_.on<rtsp_response>([this](const rtsp_response& response) {
            emit(response);
        });
    }

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

            asio::async_connect(
                socket_, results,
                [this](const asio::error_code connect_error, const asio::ip::tcp::endpoint& endpoint) {
                    if (connect_error) {
                        RAV_ERROR("Failed to connect: {}", connect_error.message());
                        return;
                    }
                    RAV_INFO("Connected to {}:{}", endpoint.address().to_string(), endpoint.port());
                    async_write();  // Schedule a write operation, in case there is data to send
                    async_read_some();
                    emit<rtsp_connect_event>(rtsp_connect_event {*this});
                }
            );
        }
    );
}

void rav::rtsp_client::async_write() {
    if (output_buffer_.exhausted()) {
        return;
    }
    asio::async_write(
        socket_, asio::buffer(output_buffer_.data()),
        [this](const asio::error_code ec, const std::size_t length) {
            if (ec) {
                RAV_ERROR("Write error: {}", ec.message());
                return;
            }
            output_buffer_.consume(length);
            if (!output_buffer_.exhausted()) {
                async_write();  // Schedule another write
            }
        }
    );
}

void rav::rtsp_client::async_read_some() {
    auto buffer = input_buffer_.prepare(512);
    socket_.async_read_some(
        asio::buffer(buffer.data(), buffer.size_bytes()),
        [this](const asio::error_code ec, const std::size_t length) mutable {
            if (ec) {
                if (ec == asio::error::operation_aborted) {
                    RAV_TRACE("Operation aborted");
                    return;
                }
                if (ec == asio::error::eof) {
                    RAV_TRACE("EOF");
                    return;
                }
                RAV_ERROR("Read error: {}", ec.message());
                return;
            }

            input_buffer_.commit(length);

            auto result = parser_.parse(input_buffer_);
            if (!(result == rtsp_parser::result::good || result == rtsp_parser::result::indeterminate)) {
                RAV_ERROR("Parsing error: {}", static_cast<int>(result));
                return;
            }

            async_read_some();
        }
    );
}
