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
#include "detail/rtsp_request.hpp"
#include "detail/rtsp_response.hpp"
#include "ravennakit/core/events.hpp"

#include <asio.hpp>
#include <unordered_map>

namespace rav {

/**
 * Server for accepting RTSP connections.
 * This class assumes a single threaded io_context and no attempt to synchronise access and callbacks have been made.
 */
class rtsp_server final: rtsp_connection::subscriber {
  public:
    using request_handler = std::function<void(rtsp_connection::request_event)>;

    /**
     * Baseclass for other classes which need to handle requests for specific paths.
     */
    class path_handler {
    public:
        virtual ~path_handler() = default;

        /**
         * Called when a request is received.
         * @param event The event containing the request.
         */
        virtual void on_request([[maybe_unused]] rtsp_connection::request_event event) const {}

        /**
         * Called when a response is received.
         * @param event The event containing the response.
         */
        virtual void on_response([[maybe_unused]] rtsp_connection::response_event event) {}
    };

    rtsp_server(asio::io_context& io_context, const asio::ip::tcp::endpoint& endpoint);
    rtsp_server(asio::io_context& io_context, const char* address, uint16_t port);
    ~rtsp_server() override;

    /**
     * @returns The port the server is listening on.
     */
    [[nodiscard]] uint16_t port() const;

    /**
     * Sets given handler to handle requests for given path.
     * @param path The path to associate the handler with. The path should NOT be uri encoded.
     * @param handler The handler to set. If the handler is nullptr it will remove any previously registered handler for
     * path.
     */
    void register_handler(const std::string& path, path_handler* handler);

    /**
     * Removes given handler from all paths.
     */
    void unregister_handler(const path_handler* handler_to_remove);

    /**
     * Sends a request to all connected clients. The path will determine which clients will receive the request.
     * @param path The path to send the request to.
     * @param request The request to send.
     */
    void send_request(const std::string& path, const rtsp_request& request) const;

    /**
     * Closes the listening socket. Implies cancellation.
     */
    void stop();

    /**
     * Resets handlers for all paths.
     */
    void reset() noexcept;

  protected:
    void on_connect(rtsp_connection& connection) override;
    void on_request(rtsp_connection& connection, const rtsp_request& request) override;
    void on_response(rtsp_connection& connection, const rtsp_response& response) override;
    void on_disconnect(rtsp_connection& connection) override;

  private:
    static constexpr auto k_special_path_all = "/all";

    struct path_context {
        path_handler* handler;
        std::vector<std::shared_ptr<rtsp_connection>> connections;
    };

    asio::ip::tcp::acceptor acceptor_;
    std::unordered_map<std::string, path_context> paths_;

    void async_accept();
};

}  // namespace rav
