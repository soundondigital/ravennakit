/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/rtsp_server.hpp"

#include "ravennakit/containers/string_stream.hpp"
#include "ravennakit/rtsp/rtsp_parser.hpp"
#include "ravennakit/rtsp/rtsp_request.hpp"
#include "ravennakit/util/exclusive_access_guard.hpp"
#include "ravennakit/util/tracy.hpp"

class rav::rtsp_server::connection: public std::enable_shared_from_this<connection> {
  public:
    explicit connection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {
        parser_.on<rtsp_response>([this](const rtsp_response& response, rtsp_parser&) {
            TRACY_ZONE_SCOPED;
            RAV_INFO("{}\n{}", response.to_debug_string(), rav::string_replace(response.data, "\r\n", "\n"));
        });
        parser_.on<rtsp_request>([this](const rtsp_request& request, rtsp_parser&) {
            TRACY_ZONE_SCOPED;
            RAV_INFO("{}\n{}", request.to_debug_string(), rav::string_replace(request.data, "\r\n", "\n"));
        });
    }

    void start() {
        TRACY_ZONE_SCOPED;
        async_read_some();  // Start reading chain
    }

  private:
    asio::ip::tcp::socket socket_;
    string_stream input_stream_;
    rtsp_parser parser_;

    void async_read_some() {
        TRACY_ZONE_SCOPED;

        auto self(shared_from_this());
        auto buffer = input_stream_.prepare(512);
        socket_.async_read_some(
            asio::buffer(buffer.data(), buffer.size_bytes()),
            [this, self](const std::error_code ec, const std::size_t bytes_transferred) {
                TRACY_ZONE_SCOPED;

                if (ec) {
                    RAV_ERROR("Read error: {}", ec.message());
                    // TODO: Close the connection?
                    return;
                }

                input_stream_.commit(bytes_transferred);

                auto result = parser_.parse(input_stream_);
                if (!(result == rtsp_parser::result::good || result == rtsp_parser::result::indeterminate)) {
                    RAV_ERROR("Parsing error: {}", static_cast<int>(result));
                    // TODO: Close the connection?
                    return;
                }

                async_read_some();
            }
        );
    }
};

rav::rtsp_server::rtsp_server(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint) :
    acceptor_(asio::make_strand(io_context), endpoint) {
    async_accept();
}

void rav::rtsp_server::close() {
    TRACY_ZONE_SCOPED;
    acceptor_.close();
}

void rav::rtsp_server::cancel() {
    TRACY_ZONE_SCOPED;
    acceptor_.cancel();
}

void rav::rtsp_server::async_accept() {
    TRACY_ZONE_SCOPED;
    acceptor_.async_accept(
        // Accepting through the strand -should- bind new sockets to this strand as well so that all operations on the
        // socket are serialized with the acceptor's strand.
        asio::make_strand(acceptor_.get_executor()),
        [this](const std::error_code ec, asio::ip::tcp::socket socket) {
            TRACY_ZONE_SCOPED;
            if (ec) {
                if (ec != asio::error::operation_aborted) {
                    RAV_ERROR("Error accepting connection: {}", ec.message());
                }
                return;
            }

            if (!acceptor_.is_open()) {
                RAV_ERROR("Acceptor is not open, cannot accept connections");
                return;
            }

            RAV_TRACE("Accepting connection from: {}", socket.remote_endpoint().address().to_string());
            auto [it, inserted] = connections_.insert(std::make_shared<connection>(std::move(socket)));
            if (!inserted) {
                RAV_ERROR("Failed to insert connection into the set");
                return;
            }
            (*it)->start();
            async_accept();
        }
    );
}
