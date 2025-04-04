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
#include "ravennakit/core/uri.hpp"

rav::rtsp::Server::~Server() {
    for (const auto& [path, ctx] : paths_) {
        for (auto& c : ctx.connections) {
            c->set_subscriber(nullptr);
        }
        // TODO: We might want to shutdown the connection here, but a shutdown when shutdown was already called before
        // will result in an exception thrown by shutdown(). We can either catch the exception or add a flag to the
        // connection to check if it was already shutdown. For now, we just ignore the shutdown.
    }
}

uint16_t rav::rtsp::Server::port() const {
    return acceptor_.local_endpoint().port();
}

void rav::rtsp::Server::register_handler(const std::string& path, PathHandler* handler) {
    if (handler == nullptr) {
        const auto found = paths_.find(path);
        if (found == paths_.end()) {
            return;
        }
        found->second.handler = nullptr;
        if (found->second.connections.empty()) {
            paths_.erase(path);
        }
        return;
    }
    auto& pc = paths_[path];
    pc.handler = handler;
}

void rav::rtsp::Server::unregister_handler(const PathHandler* handler_to_remove) {
    for (auto it = paths_.begin(); it != paths_.end();) {
        if (it->second.handler == handler_to_remove) {
            it->second.handler = nullptr;
            if (it->second.connections.empty()) {
                it = paths_.erase(it);
            } else {
                ++it;
            }
        } else {
            ++it;
        }
    }
}

size_t rav::rtsp::Server::send_request(const std::string& path, const Request& request) const {
    const auto found = paths_.find(path);
    if (found == paths_.end()) {
        RAV_WARNING("No path context for path: {} (request={})", path, request.to_debug_string(false));
        return 0;
    }
    size_t count = 0;
    for (auto& c : found->second.connections) {
        c->async_send_request(request);
        count++;
    }
    return count;
}

rav::rtsp::Server::Server(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint) :
    acceptor_(io_context, endpoint) {
    async_accept();
}

rav::rtsp::Server::Server(asio::io_context& io_context, const char* address, const uint16_t port) :
    Server(io_context, asio::ip::tcp::endpoint(asio::ip::make_address(address), port)) {}

void rav::rtsp::Server::stop() {
    TRACY_ZONE_SCOPED;
    acceptor_.cancel();
    for (const auto& [path, ctx] : paths_) {
        for (auto& c : ctx.connections) {
            c->set_subscriber(nullptr);
            c->shutdown();
        }
    }
}

void rav::rtsp::Server::reset() noexcept {
    for (auto& [path, ctx] : paths_) {
        ctx.handler = nullptr;
    }
}

void rav::rtsp::Server::on_connect(Connection& connection) {
    RAV_TRACE("New connection from: {}", connection.remote_endpoint().address().to_string());
}

void rav::rtsp::Server::on_request(Connection& connection, const Request& request) {
    RAV_TRACE("Received request: {}", request.to_debug_string(false));
    const auto uri = Uri::parse(request.uri);

    auto& pc = paths_[uri.path];

    if (pc.add_connection_if_not_exists(connection)) {
        RAV_TRACE(
            "Added connection from {}:{} to {}", connection.remote_endpoint().address().to_string(),
            connection.remote_endpoint().port(), uri.path
        );
    }

    if (pc.handler) {
        pc.handler->on_request({connection, request});
    } else {
        RAV_WARNING("No handler registered for uri: {}", uri.to_string());
        connection.async_send_response(Response(404, "Not Found", "No handler registered for URI"));
        // Leave the connection open so that we can push an update when a handler is registered
    }
}

void rav::rtsp::Server::on_response([[maybe_unused]] Connection& connection, const Response& response) {
    RAV_TRACE("Received response: {}", response.to_debug_string(false));
}

void rav::rtsp::Server::on_disconnect(Connection& connection) {
    RAV_TRACE("Connection closed: {}", connection.remote_endpoint().address().to_string());

    for (auto& [path, ctx] : paths_) {
        ctx.connections.erase(
            std::remove_if(
                ctx.connections.begin(), ctx.connections.end(),
                [&connection](const auto& c) {
                    return c.get() == &connection;
                }
            ),
            ctx.connections.end()
        );
    }
}

bool rav::rtsp::Server::PathContext::add_connection_if_not_exists(Connection& connection) {
    auto shared = connection.shared_from_this();
    if (!shared) {
        RAV_ERROR("Failed to create shared connection");
        return false;
    }
    if (std::find(connections.begin(), connections.end(), shared) == connections.end()) {
        connections.push_back(std::move(shared));
        return true;
    }
    return false;
}

void rav::rtsp::Server::async_accept() {
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

        auto& ctx = paths_[k_special_path_all];  // All connections get added to the special path /all
        const auto& it = ctx.connections.emplace_back(Connection::create(std::move(socket)));
        it->set_subscriber(this);
        it->start();
        on_connect(*it);
        async_accept();
    });
}
