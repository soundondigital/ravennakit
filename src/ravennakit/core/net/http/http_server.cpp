/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/net/http/http_server.hpp"

#include "ravennakit/core/log.hpp"

#include <boost/beast/version.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/message_generator.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/system/result.hpp>

class rav::HttpServer::ClientSession: public std::enable_shared_from_this<ClientSession> {
  public:
    ClientSession() = delete;

    explicit ClientSession(boost::asio::ip::tcp::socket&& socket, HttpServer* owner) :
        stream_(std::move(socket)), owner_(owner) {
        RAV_ASSERT(owner != nullptr, "Owner cannot be null");
    }

    void start() {
        boost::asio::dispatch(
            stream_.get_executor(), boost::beast::bind_front_handler(&ClientSession::do_read, shared_from_this())
        );
    }

    void set_owner(HttpServer* owner) {
        owner_ = owner;
        if (owner_ == nullptr && stream_.socket().is_open()) {
            do_close();
        }
    }

    std::string get_remote_address_string() const {
        boost::beast::error_code ec;
        const auto endpoint = stream_.socket().remote_endpoint(ec);
        if (ec) {
            return {ec.message()};
        }
        return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
    }

  private:
    boost::beast::tcp_stream stream_;
    HttpServer* owner_ {};
    boost::beast::flat_buffer buffer_;
    boost::beast::http::request<boost::beast::http::string_body> request_;

    void do_read() {
        if (owner_ == nullptr) {
            return;  // Acceptor was stopped
        }

        // Make the request empty before reading, otherwise the operation behavior is undefined.
        request_ = {};

        stream_.expires_after(std::chrono::seconds(k_timeout_seconds));

        // Read a request
        boost::beast::http::async_read(
            stream_, buffer_, request_, boost::beast::bind_front_handler(&ClientSession::on_read, shared_from_this())
        );
    }

    void on_read(const boost::beast::error_code& ec, std::size_t bytes_transferred) {
        std::ignore = bytes_transferred;

        if (owner_ == nullptr) {
            return;  // Acceptor was stopped
        }

        // This means they closed the connection
        if (ec == boost::beast::http::error::end_of_stream) {
            return do_close();
        }

        if (ec) {
            owner_->on_client_error(ec, "read");
            return do_close();
        }

        // Send the response
        send_response(owner_->on_request(std::move(request_)));
    }

    void send_response(boost::beast::http::message_generator&& msg) {
        if (owner_ == nullptr) {
            return;  // Acceptor was stopped
        }

        bool keep_alive = msg.keep_alive();

        // Write the response
        boost::beast::async_write(
            stream_, std::move(msg),
            boost::beast::bind_front_handler(&ClientSession::on_write, shared_from_this(), keep_alive)
        );
    }

    void on_write(const bool keep_alive, const boost::beast::error_code& ec, std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if (owner_ == nullptr) {
            return;  // Acceptor was stopped
        }

        if (ec) {
            owner_->on_client_error(ec, "write");
            do_close();
            return;
        }

        if (!keep_alive) {
            // This means we should close the connection, usually because the response indicated the "Connection: close"
            // semantic.
            return do_close();
        }

        // Read another request
        do_read();
    }

    void do_close() {
        if (stream_.socket().is_open()) {
            boost::beast::error_code ec;
            stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            if (ec && ec != boost::beast::errc::not_connected) {
                if (owner_ != nullptr) {
                    owner_->on_client_error(ec, "shutdown");
                }
            }
        }

        if (owner_ == nullptr) {
            return;  // Acceptor was stopped
        }

        owner_->remove_client_session(this);
    }
};

class rav::HttpServer::Listener: public std::enable_shared_from_this<Listener> {
  public:
    explicit Listener(boost::asio::io_context& io_context, HttpServer* owner) :
        io_context_(io_context), owner_(owner), acceptor_(io_context) {
        RAV_ASSERT(owner != nullptr, "Owner cannot be null");
    }

    boost::system::result<void> start(const boost::asio::ip::tcp::endpoint& endpoint) {
        boost::system::error_code ec;
        acceptor_.open(endpoint.protocol(), ec);
        if (ec) {
            return ec;
        }

        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if (ec) {
            return ec;
        }

        acceptor_.bind(endpoint, ec);
        if (ec) {
            return ec;
        }

        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec) {
            return ec;
        }

        do_accept();

        return {};
    }

    void set_owner(HttpServer* owner) {
        owner_ = owner;

        if (owner_ == nullptr && acceptor_.is_open()) {
            boost::beast::error_code ec;
            acceptor_.close(ec);
            if (ec) {
                RAV_ERROR("Error closing acceptor: {}", ec.message());
            }
        }
    }

    boost::asio::ip::tcp::endpoint local_endpoint() const {
        return acceptor_.local_endpoint();
    }

  private:
    boost::asio::io_context& io_context_;
    HttpServer* owner_ {};
    boost::asio::ip::tcp::acceptor acceptor_;

    void do_accept() {
        acceptor_.async_accept(io_context_, boost::beast::bind_front_handler(&Listener::on_accept, shared_from_this()));
    }

    void on_accept(const boost::beast::error_code& ec, boost::asio::ip::tcp::socket socket) {
        if (owner_ == nullptr) {
            return;  // Acceptor was stopped
        }

        if (ec) {
            owner_->on_listener_error(ec, "accept");
            return;  // To avoid infinite loop
        }

        owner_->on_accept(std::move(socket));
        do_accept();
    }
};

rav::HttpServer::HttpServer(boost::asio::io_context& io_context) : io_context_(io_context) {}

rav::HttpServer::~HttpServer() {
    stop();
}

boost::system::result<void> rav::HttpServer::start(const std::string_view bind_address, uint16_t port) {
    if (listener_ != nullptr) {
        return boost::asio::error::already_started;
    }

    boost::system::error_code ec;
    auto addr = boost::asio::ip::make_address(bind_address, ec);
    if (ec) {
        return ec;
    }

    auto listener = std::make_shared<Listener>(io_context_, this);

    const auto result = listener->start({addr, port});
    if (result.has_error()) {
        return result;
    }

    listener_ = std::move(listener);

    return {};
}

void rav::HttpServer::stop() {
    if (listener_) {
        listener_->set_owner(nullptr);
        listener_.reset();
    }

    for (const auto& session : client_sessions_) {
        session->set_owner(nullptr);
    }

    client_sessions_.clear();
}

boost::asio::ip::tcp::endpoint rav::HttpServer::get_local_endpoint() const {
    return listener_ ? listener_->local_endpoint() : boost::asio::ip::tcp::endpoint {};
}

std::string rav::HttpServer::get_address_string() const {
    const auto endpoint = get_local_endpoint();
    return endpoint.address().to_string() + ":" + std::to_string(endpoint.port());
}

size_t rav::HttpServer::get_client_count() const {
    return client_sessions_.size();
}

void rav::HttpServer::on_accept(boost::asio::ip::tcp::socket socket) {
    auto session = std::make_shared<ClientSession>(std::move(socket), this);
    session->start();
    RAV_TRACE("Accepted new connection from {}", session->get_remote_address_string());
    client_sessions_.push_back(std::move(session));
}

void rav::HttpServer::on_listener_error(const boost::beast::error_code& ec, std::string_view what) const {
    RAV_ERROR("Listener error: {}: {}", what, ec.message());
}

void rav::HttpServer::on_client_error(const boost::beast::error_code& ec, std::string_view what) const {
    if (ec == boost::beast::error::timeout) {
        RAV_TRACE("Client timeout: {}", ec.message());
    } else {
        RAV_ERROR("Client error: {}: {}", what, ec.message());
    }
}

boost::beast::http::message_generator
rav::HttpServer::on_request(const boost::beast::http::request<boost::beast::http::string_body>& request) {
    if (!request.body().empty()) {
        RAV_INFO("{} {} {}", request.method_string(), request.target(), request.body().c_str());
    } else {
        RAV_INFO("{} {}", request.method_string(), request.target());
    }

    try {
        PathMatcher::Parameters parameters;

        if (const auto* match = router_.match(request.method(), request.target(), &parameters)) {
            Response response;
            response.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            response.keep_alive(request.keep_alive());
            response.version(request.version());
            (*match)(request, response, parameters);
            auto result_int = response.result_int();
            if (result_int >= 200 && result_int < 300) {
                RAV_INFO(
                    "{} {} {}", result_int, response.reason(),
                    !response.body().empty() ? rav::string_replace(response.body(), "\r\n", "<crlf>") : ""
                );
            } else {
                RAV_WARNING(
                    "{} {} {}", result_int, response.reason(), !response.body().empty() ? response.body().c_str() : ""
                );
            }
            return response;
        }

        Response res {boost::beast::http::status::not_found, request.version()};
        res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(boost::beast::http::field::content_type, "text/html");
        res.keep_alive(request.keep_alive());
        res.body() = std::string("No matching handler");
        res.prepare_payload();
        RAV_WARNING("Response: {} {}", res.result_int(), res.reason());
        return res;
    } catch (const std::exception& e) {
        RAV_ERROR("Exception in handler: {}", e.what());
        Response res {boost::beast::http::status::internal_server_error, request.version()};
        res.set(boost::beast::http::field::content_type, "text/plain");
        res.body() = "Internal server error";
        res.prepare_payload();
        RAV_WARNING("Response: {} {}", res.result_int(), res.reason());
        return res;
    }
}

void rav::HttpServer::remove_client_session(const ClientSession* session) {
    client_sessions_.erase(
        std::remove_if(
            client_sessions_.begin(), client_sessions_.end(),
            [session](const auto& s) {
                if (s.get() == session) {
                    RAV_TRACE("Removing client session");
                    return true;
                }
                return false;
            }
        ),
        client_sessions_.end()
    );
}
