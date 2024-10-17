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

#include <asio.hpp>
#include <set>

namespace rav {

class rtsp_server {
  public:
    rtsp_server(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint);

    [[nodiscard]] uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

    void close();
    void cancel();

  private:
    class connection;

    asio::ip::tcp::acceptor acceptor_;
    std::set<std::shared_ptr<connection>> connections_;

    void async_accept();
};

}  // namespace rav
