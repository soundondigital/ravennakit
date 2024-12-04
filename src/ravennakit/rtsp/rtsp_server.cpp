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

#include "ravennakit/core/containers/string_buffer.hpp"
#include "ravennakit/core/util/todo.hpp"
#include "ravennakit/rtsp/detail/rtsp_request.hpp"
#include "ravennakit/core/exclusive_access_guard.hpp"
#include "ravennakit/core/tracy.hpp"

rav::rtsp_server::~rtsp_server() {
    for (const auto& c : connections_) {
        c->set_subscriber(nullptr);
        // TODO: We might want to shutdown the connection here, but a shutdown when shutdown was already called before
        // will result in an exception thrown by shutdown(). We can either catch the exception or add a flag to the
        // connection to check if it was already shutdown. For now, we just ignore the shutdown.
    }
}

uint16_t rav::rtsp_server::port() const {
    return acceptor_.local_endpoint().port();
}

rav::rtsp_server::rtsp_server(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint) :
    acceptor_(io_context, endpoint) {
    async_accept();
}

rav::rtsp_server::rtsp_server(asio::io_context& io_context, const char* address, const uint16_t port) :
    rtsp_server(io_context, asio::ip::tcp::endpoint(asio::ip::make_address(address), port)) {}

void rav::rtsp_server::stop() {
    TRACY_ZONE_SCOPED;
    acceptor_.cancel();
    for (const auto& c : connections_) {
        c->set_subscriber(nullptr);
        c->shutdown();
    }
}

void rav::rtsp_server::reset() noexcept {
    events_.reset();
}

void rav::rtsp_server::on_connect(rtsp_connection& connection) {
    events_.emit(rtsp_connection::connect_event {connection});
}

void rav::rtsp_server::on_request(const rtsp_request& request, rtsp_connection& connection) {
    events_.emit(rtsp_connection::request_event {request, connection});
}

void rav::rtsp_server::on_response(const rtsp_response& response, rtsp_connection& connection) {
    events_.emit(rtsp_connection::response_event {response, connection});
}

void rav::rtsp_server::async_accept() {
    acceptor_.async_accept(acceptor_.get_executor(), [this](const std::error_code ec, asio::ip::tcp::socket socket) {
        TRACY_ZONE_SCOPED;
        if (ec) {
            if (ec == asio::error::operation_aborted) {
                RAV_TRACE("Operation aborted");
                return;
            }
            if (ec == asio::error::eof) {
                RAV_TRACE("EOF");
                return;
            }
            RAV_ERROR("Read error: {}", ec.message());
            return;
        }

        if (!acceptor_.is_open()) {
            RAV_ERROR("Acceptor is not open, cannot accept connections");
            return;
        }

        RAV_TRACE(
            "Accepted new connection from: {}:{}", socket.remote_endpoint().address().to_string(),
            socket.remote_endpoint().port()
        );

        const auto& it = connections_.emplace_back(rtsp_connection::create(std::move(socket)));
        it->set_subscriber(this);
        it->start();
        on_connect(*it);
        async_accept();
    });
}
