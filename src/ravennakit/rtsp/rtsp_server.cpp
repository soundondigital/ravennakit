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

#include "ravennakit/rtsp/rtsp_request.hpp"
#include "ravennakit/rtsp/rtsp_request_parser.hpp"

class rav::rtsp_server::connection: public std::enable_shared_from_this<connection> {
  public:
    explicit connection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {
        do_read();  // Start reading chain
    }

  private:
    asio::ip::tcp::socket socket_;
    std::string input_data_ {};
    rtsp_request request_;
    rtsp_request_parser request_parser_ {request_};

    void do_read() {
        auto self(shared_from_this());
        socket_.async_read_some(
            asio::buffer(input_data_),
            [this, self](std::error_code ec, std::size_t bytes_transferred) {
                if (!ec) {
                    // TODO: Parse
                    auto [result, begin] = request_parser_.parse(input_data_.begin(), input_data_.end());

                    do_read();
                }
            }
        );
    }
};

rav::rtsp_server::rtsp_server(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint) :
    acceptor_(asio::make_strand(io_context), endpoint) {}

void rav::rtsp_server::async_accept() {
    acceptor_.async_accept(
        // Accepting through the strand -should- bind new sockets to this strand so that all operations on the socket
        // are serialized.
        asio::make_strand(acceptor_.get_executor()),
        [this](const std::error_code ec, asio::ip::tcp::socket socket) {
            if (!acceptor_.is_open()) {
                return;
            }

            if (!ec) {
                RAV_TRACE("Accepting connection from: {}", socket.remote_endpoint().address().to_string());
                connections_.insert(std::make_shared<connection>(std::move(socket)));
            } else {
                if (ec != asio::error::operation_aborted) {
                    RAV_ERROR("Error accepting connection: {}", ec.message());
                }
            }

            async_accept();
        }
    );
}
