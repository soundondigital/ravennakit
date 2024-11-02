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

#include "ravennakit/containers/string_buffer.hpp"
#include "ravennakit/rtsp/rtsp_parser.hpp"
#include "ravennakit/rtsp/rtsp_request.hpp"
#include "ravennakit/util/exclusive_access_guard.hpp"
#include "ravennakit/util/tracy.hpp"

class rav::rtsp_server::connection_impl final:
    public rtsp_connection,
    public std::enable_shared_from_this<connection_impl> {
  public:
    explicit connection_impl(asio::ip::tcp::socket socket, rtsp_server* owner) :
        rtsp_connection(std::move(socket)), owner_(owner) {}

    void start() {
        on_connected();
        async_read_some();  // Start reading chain
    }

    /**
     * Sets owner to nullptr, so that the connection cannot emit events anymore.
     */
    void reset() {
        owner_ = nullptr;
    }

  protected:
    void on_connected() override {
        if (owner_) {
            owner_->events_.emit(connection_event {*this});
        }
    }

    void on_rtsp_request(const rtsp_request& request) override {
        if (owner_) {
            owner_->events_.emit(request_event {request, *this});
        }
    }

    void on_rtsp_response(const rtsp_response& response) override {
        if (owner_) {
            owner_->events_.emit(response_event {response, *this});
        }
    }

  private:
    rtsp_server* owner_ {};
};

rav::rtsp_server::~rtsp_server() {
    for (auto& c : connections_) {
        c->reset();
        c->shutdown();
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

void rav::rtsp_server::close() {
    TRACY_ZONE_SCOPED;
    acceptor_.close();

    for (const auto& c : connections_) {
        c->reset();
        c->shutdown();
    }
}

void rav::rtsp_server::cancel() {
    TRACY_ZONE_SCOPED;
    acceptor_.cancel();
}

void rav::rtsp_server::async_accept() {
    TRACY_ZONE_SCOPED;
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

        const auto& it = connections_.emplace_back(std::make_unique<connection_impl>(std::move(socket), this));
        it->start();
        async_accept();
    });
}
