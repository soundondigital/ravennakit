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

rav::rtsp_client::rtsp_client(asio::io_context& io_context) : socket_(asio::make_strand(io_context)) {
    parser_.on<rtsp_response>([this](const rtsp_response& response, rtsp_parser&) {
        emit(response);
    });
    parser_.on<rtsp_request>([this](const rtsp_request& request, rtsp_parser&) {
        emit(request);
    });
}

void rav::rtsp_client::connect(const asio::ip::tcp::endpoint& endpoint) {
    asio::post(socket_.get_executor(), [this, endpoint]() {
        async_connect(endpoint);
    });
}

void rav::rtsp_client::connect(const std::string& addr, uint16_t port) {
    connect({asio::ip::make_address(addr), port});
}

void rav::rtsp_client::describe(const std::string& path) {
    if (!starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    asio::post(socket_.get_executor(), [this, path] {
        rtsp_request request;
        request.method = "DESCRIBE";
        request.uri = fmt::format("rtsp://{}{}", socket_.remote_endpoint().address().to_string(), path);
        request.headers["CSeq"] = "15";
        request.headers["Accept"] = "application/sdp";
        const auto encoded = request.encode();
        RAV_TRACE("Sending request: {}", request.to_debug_string());
        const bool should_trigger_async_write = output_buffer_.exhausted();
        output_buffer_.write(encoded);
        if (should_trigger_async_write) {
            async_write();
        }
    });
}

void rav::rtsp_client::setup(const std::string& path) {
    if (!starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    asio::post(socket_.get_executor(), [this, path] {
        rtsp_request request;
        request.method = "SETUP";
        request.uri = fmt::format("rtsp://{}{}", socket_.remote_endpoint().address().to_string(), path);
        request.headers["CSeq"] = "15";
        // request.headers["Session"] = "47112344";
        request.headers["Transport"] = "RTP/AVP;unicast;client_port=5004-5005";

        const auto encoded = request.encode();
        RAV_TRACE("Sending request: {}", request.to_debug_string());
        const bool should_trigger_async_write = output_buffer_.exhausted();
        output_buffer_.write(encoded);
        if (should_trigger_async_write) {
            async_write();
        }
    });
}

void rav::rtsp_client::play(const std::string& path) {
    if (!starts_with(path, "/")) {
        RAV_THROW_EXCEPTION("Path must start with a /");
    }

    asio::post(socket_.get_executor(), [this, path] {
        rtsp_request request;
        request.method = "PLAY";
        request.uri = fmt::format("rtsp://{}{}", socket_.remote_endpoint().address().to_string(), path);
        request.headers["CSeq"] = "15";
        request.headers["Transport"] = "RTP/AVP;unicast;client_port=5004-5005";

        const auto encoded = request.encode();
        RAV_TRACE("Sending request: {}", request.to_debug_string());
        const bool should_trigger_async_write = output_buffer_.exhausted();
        output_buffer_.write(encoded);
        if (should_trigger_async_write) {
            async_write();
        }
    });
}

void rav::rtsp_client::post(std::function<void()> work) {
    asio::post(socket_.get_executor(), std::move(work));
}

void rav::rtsp_client::async_connect(const asio::ip::tcp::endpoint& endpoint) {
    socket_.async_connect(endpoint, [this](const asio::error_code ec) {
        if (ec) {
            RAV_ERROR("Connect error: {}", ec.message());
            return;
        }
        RAV_INFO("Connected to {}", socket_.remote_endpoint().address().to_string());
        async_write();  // Schedule a write in case there is data to send
        async_read_some();
        emit<rtsp::connect_event>(rtsp::connect_event {});
    });
}

void rav::rtsp_client::async_write() {
    if (output_buffer_.exhausted()) {
        return;
    }
    asio::async_write(
        socket_, asio::buffer(output_buffer_.data()),
        [this](const asio::error_code ec, std::size_t length) {
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
                RAV_ERROR("Read error: {}", ec.message());
                // TODO: Close the connection?
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
