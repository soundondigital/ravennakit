/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/core/net/http/detail/http_fmt_adapters.hpp"

#include <boost/asio.hpp>
#include <boost/url.hpp>

#include <string>

namespace rav {

namespace http = boost::beast::http;
using tcp = boost::asio::ip::tcp;

/**
 * A high level wrapper around boost::beast for making HTTP requests.
 */
class HttpClient {
  public:
    /// When no port is specified in the urls, the default port is used.
    static constexpr auto k_default_port = "80";

    using Request = http::request<http::string_body>;
    using Response = http::response<http::string_body>;

    /// Callback type for async requests.
    using CallbackType = std::function<void(boost::system::result<http::response<http::string_body>> response)>;

    /**
     * Constructs a new HttpClient using the given io_context, but no url.
     * @param io_context The io_context to use for the request.
     */
    explicit HttpClient(boost::asio::io_context& io_context);

    /**
     * Constructs a new HttpClient using the given io_context and url.
     * @param io_context The io_context to use for the request.
     * @param url The url to request.
     */
    HttpClient(boost::asio::io_context& io_context, std::string_view url);

    /**
     * Constructs a new HttpClient using the given io_context and url.
     * @param io_context The io_context to use for the request.
     * @param url The url to request.
     */
    HttpClient(boost::asio::io_context& io_context, const boost::urls::url& url);

    /**
     * Constructs a new HttpClient using the given io_context and url.
     * @param io_context The io_context to use for the request.
     * @param endpoint The endpoint to request.
     */
    HttpClient(boost::asio::io_context& io_context, const boost::asio::ip::tcp::endpoint& endpoint);

    /**
     * Constructs a new HttpClient using the given io_context and url.
     * @param io_context The io_context to use for the request.
     * @param address The address to request.
     * @param port The port to request.
     */
    HttpClient(boost::asio::io_context& io_context, const boost::asio::ip::address& address, uint16_t port);

    ~HttpClient();

    /**
     * Sets the host to connect to.
     * @param url The url with the host info.
     */
    void set_host(const boost::urls::url& url);

    /**
     * Sets the host to connect to.
     * @param url The host to connect to.
     */
    void set_host(std::string_view url);

    /**
     * Sets the host to connect to.
     * @param host The host to connect to.
     * @param service The service (port) to connect to.
     * @param target The target to connect to.
     */
    void set_host(std::string_view host, std::string_view service, std::string_view target = {});

    /**
     * Asynchronous GET request.
     * The callback's lifetime will be tied to the io_context so make sure referenced objects are kept alive until the
     * callback is called.
     * @param target The target to request.
     * @param callback The callback to call when the request is complete.
     */
    void get_async(std::string_view target, CallbackType callback);

    /**
     * Synchronous POST request to the target of the URL, or the root if no target is specified.
     * @return The response from the server, which may contain an error.
     */
    void post_async(
        std::string_view target, std::string body, CallbackType callback,
        std::string_view content_type = "application/json"
    );

    /**
     * Asynchronous request.
     * @param method The HTTP method to use for the request.
     * @param target The target to request.
     * @param body The optional body to send with the request.
     * @param content_type
     * @param callback The callback to call when the request is complete.
     */
    void request_async(
        http::verb method, std::string_view target, std::string body, std::string_view content_type,
        CallbackType callback
    );

    /**
     * Clears all scheduled requests if there are any. Otherwise, this function has no effect.
     */
    void cancel_outstanding_requests() {
        requests_ = {};
    }

  private:
    /**
     * A session class that keeps itself alive and handles the connection and request/response cycle.
     */
    class Session: public std::enable_shared_from_this<Session> {
      public:
        enum class State {
            disconnected, resolving, connecting, connected, waiting_for_send, waiting_for_response
        };
        explicit Session(boost::asio::io_context& io_context, HttpClient* owner);

        void send_requests();
        void clear_owner();

      private:
        HttpClient* owner_ = nullptr;
        boost::asio::ip::tcp::resolver resolver_;
        boost::beast::tcp_stream stream_;
        http::response<http::string_body> response_;
        boost::beast::flat_buffer buffer_;
        State state_ = State::disconnected;

        void async_connect();
        void async_send();
        void on_resolve(const boost::beast::error_code& ec, const tcp::resolver::results_type& results);
        void on_connect(const boost::beast::error_code& ec, const tcp::resolver::results_type::endpoint_type&);
        void on_write(const boost::beast::error_code& ec, std::size_t bytes_transferred);
        void on_read(boost::beast::error_code ec, std::size_t bytes_transferred);
    };

    boost::asio::io_context& io_context_;
    std::string host_;
    std::string service_;
    std::string target_;
    std::queue<std::pair<http::request<http::string_body>, CallbackType>> requests_;
    std::shared_ptr<Session> session_;
};

}  // namespace rav
