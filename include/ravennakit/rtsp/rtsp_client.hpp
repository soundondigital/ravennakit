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

#include <asio.hpp>

namespace rav {

/**
 * Client for connecting to an RTSP server. Given io_context must be single-threaded to implicitly support
 * thread-safety.
 */
class rtsp_client final: rtsp_connection::subscriber {
  public:
    using events_type =
        events<rtsp_connection::connect_event, rtsp_connection::response_event, rtsp_connection::request_event>;

    explicit rtsp_client(asio::io_context& io_context);
    ~rtsp_client() override;

    rtsp_client(const rtsp_client&) = delete;
    rtsp_client& operator=(const rtsp_client&) = delete;

    rtsp_client(rtsp_client&&) noexcept = default;
    rtsp_client& operator=(rtsp_client&&) noexcept = default;

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
    void async_describe(const std::string& path, std::string data = {}) const;

    /**
     * Sends a SETUP request to the server. Function is async and will return immediately.
     * @param path The path to setup.
     */
    void async_setup(const std::string& path) const;

    /**
     * Sends a PLAY request to the server. Function is async and will return immediately.
     * @param path The path for the PLAY command.
     */
    void async_play(const std::string& path) const;

    /**
     * Sends a TEARDOWN request to the server. Function is async and will return immediately.
     * @param path The path for the TEARDOWN command.
     */
    void async_teardown(const std::string& path) const;

    /**
     * Sends given response to the server. Function is async and will return immediately.
     * @param response The response to send.
     */
    void async_send_response(const rtsp_response& response) const;

    /**
     * Sends given request to the server. Function is async and will return immediately.
     * @param request The request to send.
     */
    void async_send_request(const rtsp_request& request) const;

    /**
     * Registers a handler for a specific event.
     * @tparam T The event type.
     * @param handler The handler to register.
     */
    template<class T>
    void on(events_type::handler<T> handler) {
        events_.on(handler);
    }

    // rtsp_connection::subscriber overrides
    void on_connect(rtsp_connection& connection) override;
    void on_request(rtsp_connection& connection, const rtsp_request& request) override;
    void on_response(rtsp_connection& connection, const rtsp_response& response) override;

  private:
    asio::ip::tcp::resolver resolver_;
    std::string host_;
    std::shared_ptr<rtsp_connection> connection_;
    events_type events_;

    void
    async_resolve_connect(const std::string& host, const std::string& service, asio::ip::resolver_base::flags flags);
};

}  // namespace rav
