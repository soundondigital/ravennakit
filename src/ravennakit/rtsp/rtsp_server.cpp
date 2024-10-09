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

rav::rtsp_server::rtsp_server(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint) :
    acceptor_(io_context, endpoint) {}
