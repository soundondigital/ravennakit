/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/core/log.hpp"

#include <asio/ip/tcp.hpp>

namespace rav {

class rtsp_server {
  public:
    rtsp_server(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint);

    [[nodiscard]] uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

  private:
    asio::ip::tcp::acceptor acceptor_;

    void async_accept() {
        acceptor_.async_accept([this](const std::error_code ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                RAV_TRACE("Accepting connection from: {}", socket.remote_endpoint().address().to_string());
            } else {
                if (ec != asio::error::operation_aborted) {
                    RAV_ERROR("Error accepting connection: {}", ec.message());
                }
            }
            async_accept();
        });
    }
};

}  // namespace rav
