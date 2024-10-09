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

class rav::rtsp_server::connection: public std::enable_shared_from_this<connection> {
  public:
    explicit connection(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {
        do_read_header();
    }

  private:
    asio::ip::tcp::socket socket_;
    std::string input_header_;
    std::vector<uint8_t> input_data_;

    void do_read_header() {
        auto self(shared_from_this());
        asio::async_read_until(
            socket_, asio::dynamic_buffer(input_header_), "\r\n\r\n",
            [this, self](const asio::error_code& ec, std::size_t length) {
                if (!ec) {
                    // TODO: Parse the header
                    do_read_body(0);  // TODO: Specify the length based on the header
                }
            }
        );
    }

    void do_read_body(const size_t length) {
        auto self(shared_from_this());
        asio::async_read(
            socket_, asio::dynamic_buffer(input_data_), asio::transfer_exactly(length),
            [this, self](const asio::error_code& ec, const std::size_t length) {
                if (!ec) {
                    // Parse the body
                    // do_write();
                }
            }
        );
    }
};

rav::rtsp_server::rtsp_server(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint) :
    acceptor_(asio::make_strand(io_context), endpoint) {}

void rav::rtsp_server::async_accept() {
    acceptor_.async_accept(
        // Accepting with a strand -should- bind new sockets to this strand so that all operations on the socket are
        // serialized.
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
