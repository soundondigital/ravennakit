/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/net/http/http_client.hpp"

#include <utility>

#include "ravennakit/core/assert.hpp"

rav::HttpClient::HttpClient(boost::asio::io_context& io_context, const std::chrono::milliseconds timeout_seconds) :
    io_context_(io_context), timeout_seconds_(timeout_seconds) {}

rav::HttpClient::HttpClient(
    boost::asio::io_context& io_context, const std::string_view url, const std::chrono::milliseconds timeout_seconds
) :
    io_context_(io_context), timeout_seconds_(timeout_seconds) {
    const boost::urls::url parsed_url(url);
    host_ = parsed_url.host();
    service_ = parsed_url.port();
}

rav::HttpClient::HttpClient(
    boost::asio::io_context& io_context, const boost::urls::url& url, const std::chrono::milliseconds timeout_seconds
) :
    io_context_(io_context), timeout_seconds_(timeout_seconds) {
    host_ = url.host();
    service_ = url.port();
}

rav::HttpClient::HttpClient(
    boost::asio::io_context& io_context, const boost::asio::ip::tcp::endpoint& endpoint,
    const std::chrono::milliseconds timeout_seconds
) :
    io_context_(io_context), timeout_seconds_(timeout_seconds) {
    host_ = endpoint.address().to_string();
    service_ = std::to_string(endpoint.port());
}

rav::HttpClient::HttpClient(
    boost::asio::io_context& io_context, const boost::asio::ip::address& address, const uint16_t port,
    const std::chrono::milliseconds timeout_seconds
) :
    io_context_(io_context), timeout_seconds_(timeout_seconds) {
    host_ = address.to_string();
    service_ = std::to_string(port);
}

rav::HttpClient::~HttpClient() {
    if (session_) {
        session_->clear_owner();
    }
}

void rav::HttpClient::set_host(const boost::urls::url& url) {
    set_host(url.host(), url.port());
}

void rav::HttpClient::set_host(const std::string_view url) {
    const boost::urls::url parsed_url(url);
    set_host(parsed_url);
}

void rav::HttpClient::set_host(const std::string_view host, const std::string_view service) {
    if (host == host_ && service == service_) {
        return;  // No change, no need to reset the session.
    }
    if (session_) {
        session_->clear_owner();  // Clear the owner to avoid dangling pointer issues.
        session_.reset();         // Reset the session to create a new one on the next request.
    }
    host_ = host;
    service_ = service;
}

void rav::HttpClient::get_async(const std::string_view target, CallbackType callback) {
    request_async(http::verb::get, target, {}, {}, std::move(callback));
}

void rav::HttpClient::post_async(
    const std::string_view target, std::string body, CallbackType callback, const std::string_view content_type
) {
    request_async(http::verb::post, target, std::move(body), content_type, std::move(callback));
}

void rav::HttpClient::request_async(
    const http::verb method, const std::string_view target, std::string body, const std::string_view content_type,
    CallbackType callback
) {
    auto request = http::request<http::string_body>(method, target.empty() ? "/" : target, 11);
    request.set(http::field::host, host_);
    request.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
    request.set(http::field::accept, "*/*");
    request.keep_alive(true);

    if (!body.empty()) {
        request.set(http::field::content_type, content_type);
        request.body() = std::move(body);
        request.prepare_payload();
    }

    requests_.emplace(std::move(request), std::move(callback));

    if (!session_) {
        session_ = std::make_shared<Session>(io_context_, this, timeout_seconds_);
    }

    session_->send_requests();
}

void rav::HttpClient::cancel_outstanding_requests() {
    requests_ = {};
}

std::string rav::HttpClient::get_host() const {
    return host_;
}

std::string rav::HttpClient::get_service() const {
    return service_;
}

rav::HttpClient::Session::Session(
    boost::asio::io_context& io_context, HttpClient* owner, const std::chrono::milliseconds timeout_seconds
) :
    owner_(owner), timeout_seconds_(timeout_seconds), resolver_(io_context), stream_(io_context) {}

void rav::HttpClient::Session::send_requests() {
    RAV_ASSERT(owner_ != nullptr, "HttpClient::Session must have an owner");
    if (owner_->requests_.empty()) {
        return;
    }
    if (state_ == State::disconnected) {
        async_connect();  // Start the connection process
    } else if (state_ == State::connected) {
        async_send();  // If already connected, just write the next request
    }
}

void rav::HttpClient::Session::clear_owner() {
    owner_ = nullptr;
}

void rav::HttpClient::Session::async_connect() {
    RAV_ASSERT(owner_ != nullptr, "HttpClient::Session must have an owner");
    resolver_.async_resolve(
        owner_->host_, owner_->service_.empty() ? k_default_port : owner_->service_,
        boost::beast::bind_front_handler(&Session::on_resolve, shared_from_this())
    );
    state_ = State::resolving;
}

void rav::HttpClient::Session::async_send() {
    // Set a timeout on the operation
    stream_.expires_after(timeout_seconds_);

    // Send the HTTP request to the remote host
    http::async_write(
        stream_, owner_->requests_.front().first,
        boost::beast::bind_front_handler(&Session::on_write, shared_from_this())
    );

    state_ = State::waiting_for_send;
}

void rav::HttpClient::Session::on_resolve(
    const boost::beast::error_code& ec, const tcp::resolver::results_type& results
) {
    if (owner_ == nullptr) {
        return;  // Session was abandoned, nothing to do.
    }

    RAV_ASSERT(!owner_->requests_.empty(), "No requests available");

    if (ec) {
        if (const auto& cb = owner_->requests_.front().second) {
            cb(ec);
        }
        if (!owner_->requests_.empty()) {
            owner_->requests_.pop();
        }
        state_ = State::disconnected;
        return;  // Error resolving the host
    }

    // Set a timeout on the operation
    stream_.expires_after(timeout_seconds_);

    // Make the connection on the IP address we get from a lookup
    stream_.async_connect(results, boost::beast::bind_front_handler(&Session::on_connect, shared_from_this()));

    state_ = State::connecting;
}

void rav::HttpClient::Session::
    on_connect(const boost::beast::error_code& ec, const tcp::resolver::results_type::endpoint_type&) {
    if (owner_ == nullptr) {
        return;  // Session was abandoned, nothing to do.
    }

    RAV_ASSERT(!owner_->requests_.empty(), "No requests available");

    if (ec) {
        state_ = State::disconnected;
        if (const auto& cb = owner_->requests_.front().second) {
            cb(ec);
        }
        if (!owner_->requests_.empty()) {
            owner_->requests_.pop();
        }
        return;
    }

    state_ = State::connected;

    async_send();  // Start writing the first request
}

void rav::HttpClient::Session::on_write(const boost::beast::error_code& ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (owner_ == nullptr) {
        return;  // Session was abandoned, nothing to do.
    }

    RAV_ASSERT(!owner_->requests_.empty(), "No requests available");

    if (ec) {
        state_ = State::disconnected;
        if (const auto& cb = owner_->requests_.front().second) {
            cb(ec);
        }
        if (!owner_->requests_.empty()) {
            owner_->requests_.pop();
        }
        state_ = State::disconnected;
        return;
    }

    response_ = {};

    // Receive the HTTP response
    http::async_read(
        stream_, buffer_, response_, boost::beast::bind_front_handler(&Session::on_read, shared_from_this())
    );

    state_ = State::waiting_for_response;
}

void rav::HttpClient::Session::on_read(boost::beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (owner_ == nullptr) {
        return;  // Session was abandoned, nothing to do.
    }

    RAV_ASSERT(!owner_->requests_.empty(), "No requests available");

    if (ec) {
        state_ = State::disconnected;
        if (const auto& cb = owner_->requests_.front().second) {
            cb(ec);
        }
        if (!owner_->requests_.empty()) {
            owner_->requests_.pop();
        }
        state_ = State::disconnected;
        return;
    }

    // Move the callback function so that the lifetime is extended to until the callback returned, in case requests are
    // cleared through cancel_outstanding_requests.
    if (const auto cb = std::move(owner_->requests_.front().second)) {
        cb(response_);
    }

    if (!owner_->requests_.empty()) {
        owner_->requests_.pop();
    }

    if (!response_.keep_alive()) {
        // Gracefully close the socket
        stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

        // not_connected happens sometimes, so don't bother reporting it.
        if (ec && ec != boost::beast::errc::not_connected) {
            RAV_ERROR("HttpClient::Session::on_read: Error closing socket: {}", ec.message());
        }

        if (!owner_->requests_.empty()) {
            async_connect();  // If there are more requests, reconnect.
            return;
        }

        state_ = State::disconnected;
        return;
    }

    if (!owner_->requests_.empty()) {
        async_send();  // If there are more requests, send the next one.
        return;
    }

    // Otherwise set the state to connected so that send_requests will schedule requests for sending.
    state_ = State::connected;
}
