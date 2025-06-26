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

#include "detail/rtsp_connection.hpp"
#include "detail/rtsp_parser.hpp"
#include "ravennakit/core/util/safe_function.hpp"

#include <boost/asio.hpp>

namespace rav::rtsp {

/**
 * Client for connecting to an RTSP server. Given io_context must be single-threaded to implicitly support
 * thread-safety.
 */
class Client final: Connection::Subscriber {
  public:
    SafeFunction<void(const Connection::ConnectEvent& event)> on_connect_event;
    SafeFunction<void(const Connection::ResponseEvent& event)> on_response_event;
    SafeFunction<void(const Connection::RequestEvent& event)> on_request_event;

    explicit Client(boost::asio::io_context& io_context);
    ~Client() override;

    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;

    Client(Client&&) noexcept = default;
    Client& operator=(Client&&) noexcept = default;

    /**
     * Connect to the given address and port. Function is async and will return immediately.
     * @param host The address to connect to.
     * @param port The port to connect to.
     */
    void async_connect(const std::string& host, uint16_t port);

    /**
     * Connect to the given address/service. Function is async and will return immediately.
     * @param host The host to connect to.
     * @param service The service to connect to.
     */
    void async_connect(const std::string& host, const std::string& service);

    /**
     * Send a DESCRIBE request to the server. Function is async and will return immediately.
     * @param path The path to describe
     * @param data The data to add.
     */
    void async_describe(const std::string& path, std::string data = {});

    /**
     * Sends a SETUP request to the server. Function is async and will return immediately.
     * @param path The path to setup.
     */
    void async_setup(const std::string& path);

    /**
     * Sends a PLAY request to the server. Function is async and will return immediately.
     * @param path The path for the PLAY command.
     */
    void async_play(const std::string& path);

    /**
     * Sends a TEARDOWN request to the server. Function is async and will return immediately.
     * @param path The path for the TEARDOWN command.
     */
    void async_teardown(const std::string& path);

    /**
     * Sends given response to the server. Function is async and will return immediately.
     * @param response The response to send.
     */
    void async_send_response(const Response& response) const;

    /**
     * Sends given request to the server. Function is async and will return immediately.
     * @param request The request to send.
     */
    void async_send_request(const Request& request) const;

    // rtsp_connection::subscriber overrides
    void on_connect(Connection& connection) override;
    void on_request(Connection& connection, const Request& request) override;
    void on_response(Connection& connection, const Response& response) override;

  private:
    boost::asio::ip::tcp::resolver resolver_;
    std::string host_;
    std::shared_ptr<Connection> connection_;
    uint32_t seq_ {0};

    void
    async_resolve_connect(const std::string& host, const std::string& service, boost::asio::ip::resolver_base::flags flags);
};

}  // namespace rav::rtsp
