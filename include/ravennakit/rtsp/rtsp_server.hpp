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

#include "rtsp_connection.hpp"
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
    struct connection_event {
        rtsp_connection& client_connection;
    };

    struct request_event {
        const rtsp_request& request;
        rtsp_connection& client_connection;
    };

    struct response_event {
        const rtsp_response& response;
        rtsp_connection& client_connection;
    };

    using events_type = events<connection_event, request_event, response_event>;

    rtsp_server(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint);
    rtsp_server(asio::io_context& io_context, const char* address, uint16_t port);
    ~rtsp_server();

    /**
     * @returns The port the server is listening on.
     */
    [[nodiscard]] uint16_t port() const;

    /**
     * Closes the listening socket. Implies cancellation.
     */
    void close();

    /**
     * Cancels all outstanding operations on the listening socket.
     */
    void cancel();

    /**
     * Registers a handler for a specific event.
     * @tparam T The event type.
     * @param handler The handler to register.
     */
    template<class T>
    void on(events<connection_event>::handler<T> handler) {
        events_.on(handler);
    }

    /**
     * Resets handlers for all events.
     */
    void reset() noexcept {
        events_.reset();
    }

  private:
    class connection_impl;

    asio::ip::tcp::acceptor acceptor_;
    std::vector<std::unique_ptr<connection_impl>> connections_;
    events_type events_;

    void async_accept();
};

}  // namespace rav
