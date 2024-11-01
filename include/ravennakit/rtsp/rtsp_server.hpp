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

#include "rtsp_request.hpp"
#include "rtsp_response.hpp"
#include "ravennakit/core/events.hpp"
#include "ravennakit/core/log.hpp"

#include <asio.hpp>
#include <set>

namespace rav {

/**
 * Server for accepting RTSP connections.
 * This class assumes a single threaded io_context and no attempt to synchronise access and callbacks have been made.
 */
class rtsp_server {
  public:
    class connection;

    struct connection_event {
        connection& connection;
    };

    struct request_event {
        const rtsp_request& request;
        connection& connection;
    };

    struct response_event {
        const rtsp_response& response;
        connection& connection;
    };

    using events_type = events<connection_event, request_event, response_event>;

    class connection {
      public:
        virtual ~connection() = default;
        virtual void async_send_response(const rtsp_response& response) = 0;
    };

    rtsp_server(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint);
    rtsp_server(asio::io_context& io_context, const char* address, uint16_t port);
    ~rtsp_server();

    [[nodiscard]] uint16_t port() const {
        return acceptor_.local_endpoint().port();
    }

    void close();
    void cancel();

    template<class T>
    void on(events<connection_event>::handler<T> handler) {
        events_.on(handler);
    }

    void reset() noexcept {
        events_.reset();
    }

  private:
    class connection_impl;

    asio::ip::tcp::acceptor acceptor_;
    std::vector<std::weak_ptr<connection_impl>> connections_;
    events_type events_;

    void async_accept();
};

}  // namespace rav
