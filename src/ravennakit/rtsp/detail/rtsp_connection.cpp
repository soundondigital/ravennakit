/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/detail/rtsp_connection.hpp"

rav::rtsp_connection::~rtsp_connection() = default;

rav::rtsp_connection::rtsp_connection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {
    parser_.on<rtsp_request>([this](const rtsp_request& request) {
        if (subscriber_) {
            subscriber_->on_request(request, *this);
        }
    });

    parser_.on<rtsp_response>([this](const rtsp_response& response) {
        if (subscriber_) {
            subscriber_->on_response(response, *this);
        }
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

void rav::rtsp_connection::start() {
    async_read_some();
}

void rav::rtsp_connection::stop() {
    socket_.close();
}

void rav::rtsp_connection::set_subscriber(subscriber* subscriber_to_set) {
    subscriber_ = subscriber_to_set;
}

void rav::rtsp_connection::async_connect(const asio::ip::tcp::resolver::results_type& results) {
    auto self = shared_from_this();
    asio::async_connect(socket_, results, [self](const asio::error_code ec, const asio::ip::tcp::endpoint& endpoint) {
        if (ec) {
            RAV_ERROR("Failed to connect: {}", ec.message());
            return;
        }
        RAV_INFO("Connected to {}:{}", endpoint.address().to_string(), endpoint.port());
        self->async_write();      // Schedule a write operation, in case there is data to send
        self->async_read_some();  // Start reading chain
        if (self->subscriber_) {
            self->subscriber_->on_connect(*self);
        }
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
    auto self = shared_from_this();
    asio::async_write(
        socket_, asio::buffer(output_buffer_.data()),
        [self](const asio::error_code ec, const std::size_t length) {
            if (ec) {
                RAV_ERROR("Write error: {}", ec.message());
                return;
            }
            self->output_buffer_.consume(length);
            if (!self->output_buffer_.exhausted()) {
                self->async_write();  // Schedule another write
            }
        }
    );
}

void rav::rtsp_connection::async_read_some() {
    auto buffer = input_buffer_.prepare(512);
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(buffer.data(), buffer.size_bytes()),
        [self](const asio::error_code ec, const std::size_t length) mutable {
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

            self->input_buffer_.commit(length);

            auto result = self->parser_.parse(self->input_buffer_);
            if (!(result == rtsp_parser::result::good || result == rtsp_parser::result::indeterminate)) {
                RAV_ERROR("Parsing error: {}", static_cast<int>(result));
                return;
            }

            self->async_read_some();
        }
    );
}
