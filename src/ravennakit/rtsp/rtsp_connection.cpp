/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/rtsp_connection.hpp"

rav::rtsp_connection::~rtsp_connection() {}

rav::rtsp_connection::rtsp_connection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {
    parser_.on<rtsp_request>([this](const rtsp_request& request) {
        on_rtsp_request(request);
    });

    parser_.on<rtsp_response>([this](const rtsp_response& response) {
        on_rtsp_response(response);
    });
}

void rav::rtsp_connection::async_send_response(const rtsp_response& response) {
    const auto encoded = response.encode();
    RAV_TRACE("Sending response: {}", response.to_debug_string(false));
    async_send_data(encoded);
}

void rav::rtsp_connection::async_send_request(const rtsp_request& request) {
    const auto encoded = request.encode();
    RAV_TRACE("Sending request: {}", request.to_debug_string(false));
    async_send_data(encoded);
}

void rav::rtsp_connection::shutdown() {
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both);
}

void rav::rtsp_connection::async_connect(const asio::ip::tcp::resolver::results_type& results) {
    asio::async_connect(socket_, results, [this](const asio::error_code ec, const asio::ip::tcp::endpoint& endpoint) {
        if (ec) {
            RAV_ERROR("Failed to connect: {}", ec.message());
            return;
        }
        RAV_INFO("Connected to {}:{}", endpoint.address().to_string(), endpoint.port());
        async_write();      // Schedule a write operation, in case there is data to send
        async_read_some();  // Start reading chain
        on_connected();
    });
}

void rav::rtsp_connection::async_send_data(const std::string& data) {
    const bool should_trigger_async_write = output_buffer_.exhausted() && socket_.is_open();
    output_buffer_.write(data);
    if (should_trigger_async_write) {
        async_write();
    }
}

void rav::rtsp_connection::async_write() {
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

void rav::rtsp_connection::async_read_some() {
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
                RAV_ERROR("Read error: {}. Closing connection.", ec.message());
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
