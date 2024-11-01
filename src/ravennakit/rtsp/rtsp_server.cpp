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

class rav::rtsp_server::connection_impl final: public connection, public std::enable_shared_from_this<connection_impl> {
  public:
    explicit connection_impl(asio::ip::tcp::socket socket, rtsp_server* owner) :
        socket_(std::move(socket)), owner_(owner) {}

    void start() {
        parser_.on<rtsp_response>([this](const rtsp_response& response) {
            owner_->events_.emit(response_event {response, *this});
        });
        parser_.on<rtsp_request>([this](const rtsp_request& request) {
            owner_->events_.emit(request_event {request, *this});
        });
        async_read_some();  // Start reading chain
    }

    void async_send_request(const rtsp_request& request) override {
        const auto encoded = request.encode();
        RAV_TRACE("Sending request: {}", request.to_debug_string(false));
        async_send_data(encoded);
    }

    void async_send_response(const rtsp_response& response) override {
        const auto encoded = response.encode();
        RAV_TRACE("Sending response: {}", response.to_debug_string(false));
        async_send_data(encoded);
    }

    /**
     * Sets owner to nullptr, so that the connection cannot emit events anymore.
     */
    void reset() {
        owner_ = nullptr;
    }

    void shutdown() {
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both);
    }

  private:
    asio::ip::tcp::socket socket_;
    rtsp_server* owner_ {};
    string_buffer input_buffer_;
    string_buffer output_buffer_;
    rtsp_parser parser_;

    void async_read_some() {
        TRACY_ZONE_SCOPED;

        auto self(shared_from_this());
        auto buffer = input_buffer_.prepare(512);
        socket_.async_read_some(
            asio::buffer(buffer.data(), buffer.size_bytes()),
            [this, self](const std::error_code ec, const std::size_t bytes_transferred) {
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
                    RAV_ERROR("Read error: {}. Closing connection.", ec.message());
                    return;
                }

                if (owner_ == nullptr) {
                    return;  // This connection was abandoned, closing.
                }

                input_buffer_.commit(bytes_transferred);

                auto result = parser_.parse(input_buffer_);
                if (!(result == rtsp_parser::result::good || result == rtsp_parser::result::indeterminate)) {
                    RAV_ERROR("Parsing error: {}. Closing connection.", static_cast<int>(result));
                    return;
                }

                async_read_some();
            }
        );
    }

    void async_send_data(const std::string& data) {
        const bool should_trigger_async_write = output_buffer_.exhausted() && socket_.is_open();
        output_buffer_.write(data);
        if (should_trigger_async_write) {
            async_write();
        }
    }

    void async_write() {
        if (output_buffer_.exhausted()) {
            return;
        }
        asio::async_write(
            socket_, asio::buffer(output_buffer_.data()),
            [this](const asio::error_code ec, const std::size_t length) {
                if (ec) {
                    RAV_ERROR("Write error: {}", ec.message());
                    return;
                }
                output_buffer_.consume(length);
                if (!output_buffer_.exhausted()) {
                    async_write();  // Schedule another write
                }
            }
        );
    }
};

rav::rtsp_server::~rtsp_server() {
    for (auto& c : connections_) {
        if (const auto shared = c.lock()) {
            shared->reset();
            shared->shutdown();
        }
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

    for (auto& c : connections_) {
        if (const auto shared = c.lock()) {
            shared->shutdown();
        }
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

        const auto new_connection = std::make_shared<connection_impl>(std::move(socket), this);
        connections_.emplace_back(new_connection);
        events_.emit(connection_event {*new_connection});
        new_connection->start();
        async_accept();
    });
}
