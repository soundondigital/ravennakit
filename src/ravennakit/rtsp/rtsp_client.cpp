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
    parser_.on<rtsp_response>([](const rtsp_response& response, rtsp_parser&) {
        RAV_INFO("Received response:\n{}", rav::string_replace(response.encode(), "\r\n", "\n"));
    });
    parser_.on<rtsp_request>([](const rtsp_request& request, rtsp_parser&) {
        RAV_INFO("Received request:\n{}", rav::string_replace(request.encode(), "\r\n", "\n"));
    });
}

void rav::rtsp_client::connect(const asio::ip::tcp::endpoint& endpoint) {
    asio::post(socket_.get_executor(), [this, endpoint]() {
        async_connect(endpoint);
    });
}

void rav::rtsp_client::async_connect(const asio::ip::tcp::endpoint& endpoint) {
    socket_.async_connect(endpoint, [this](const asio::error_code ec) {
        if (ec) {
            RAV_ERROR("Connect error: {}", ec.message());
            return;
        }
        RAV_INFO("Connected to {}", socket_.remote_endpoint().address().to_string());
        async_write_request();
        async_read_some();
    });
}

void rav::rtsp_client::async_write_request() {
    rtsp_request request;
    request.method = "DESCRIBE";
    request.uri = "rtsp://192.168.16.51/by-id/13";
    request.rtsp_version_major = 1;
    request.rtsp_version_minor = 0;
    request.headers["CSeq"] = "15";
    request.headers["Accept"] = "application/sdp";
    auto data = request.encode();

    asio::async_write(socket_, asio::buffer(data), [](const asio::error_code ec, std::size_t length) {
        if (ec) {
            RAV_ERROR("Write error: {}", ec.message());
            return;
        }
        RAV_INFO("Wrote {} bytes", length);
    });
}

void rav::rtsp_client::async_read_some() {
    auto buffer = input_stream_.prepare(512);
    socket_.async_read_some(
        asio::buffer(buffer.data(), buffer.size_bytes()),
        [this](const asio::error_code ec, const std::size_t length) mutable {
            if (ec) {
                RAV_ERROR("Read error: {}", ec.message());
                // TODO: Close the connection?
                return;
            }

            input_stream_.commit(length);

            auto result = parser_.parse(input_stream_);
            if (!(result == rtsp_parser::result::good || result == rtsp_parser::result::indeterminate)) {
                RAV_ERROR("Parsing error: {}", static_cast<int>(result));
                return;
            }

            if (input_stream_.empty()) {
                input_stream_.reset();
            }

            async_read_some();
        }
    );
}
