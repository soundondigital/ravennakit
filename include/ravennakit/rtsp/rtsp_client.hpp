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

#include "rtsp_response_parser.hpp"

#include <asio.hpp>

namespace rav {

class rtsp_client {
  public:
    explicit rtsp_client(asio::io_context& io_context);

    void connect(const asio::ip::tcp::endpoint& endpoint);

  private:
    asio::ip::tcp::socket socket_;
    std::string input_data_;
    rtsp_response response_;
    rtsp_response_parser response_parser_ {response_};

    void async_connect(const asio::ip::tcp::endpoint& endpoint);
    void async_write_request();
    void async_read_some();
};

}  // namespace rav
