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

#include <boost/asio.hpp>
#include <unordered_map>

namespace rav::rtsp {

/**
 * Server for accepting RTSP connections.
 * This class assumes a single threaded io_context and no attempt to synchronise access and callbacks have been made.
 */
class Server final: Connection::Subscriber {
  public:
    using RequestHandler = std::function<void(Connection::RequestEvent)>;

    /**
     * Baseclass for other classes which need to handle requests for specific paths.
     */
    class PathHandler {
      public:
        virtual ~PathHandler() = default;

        /**
         * Called when a request is received.
         * @param event The event containing the request.
         */
        virtual void on_request([[maybe_unused]] Connection::RequestEvent event) const {}

        /**
         * Called when a response is received.
         * @param event The event containing the response.
         */
        virtual void on_response([[maybe_unused]] Connection::ResponseEvent event) {}
    };

    Server(boost::asio::io_context& io_context, const boost::asio::ip::tcp::endpoint& endpoint);
    Server(boost::asio::io_context& io_context, const char* address, uint16_t port);
    ~Server() override;

    /**
     * @returns The port the server is listening on.
     */
    [[nodiscard]] uint16_t port() const;

    /**
     * Sets given handler to handle requests for given path.
     * @param path The path to associate the handler with. The path should NOT be uri encoded.
     * @param handler The handler to set. If the handler is nullptr it will remove any previously registered handler
     * for path.
     */
    void register_handler(const std::string& path, PathHandler* handler);

    /**
     * Removes given handler from all paths.
     */
    void unregister_handler(const PathHandler* handler_to_remove);

    /**
     * Sends a request to all connected clients. The path will determine which clients will receive the request.
     * @param path The path to send the request to.
     * @param request The request to send.
     * @returns The number of clients that the request was sent to.
     */
    [[nodiscard]] size_t send_request(const std::string& path, const Request& request) const;

    /**
     * Closes the listening socket. Implies cancellation.
     */
    void stop();

    /**
     * Resets handlers for all paths.
     */
    void reset() noexcept;

  protected:
    void on_connect(Connection& connection) override;
    void on_request(Connection& connection, const Request& request) override;
    void on_response(Connection& connection, const Response& response) override;
    void on_disconnect(Connection& connection) override;

  private:
    static constexpr auto k_special_path_all = "/all";

    struct PathContext {
        PathHandler* handler;
        std::vector<std::shared_ptr<Connection>> connections;

        bool add_connection_if_not_exists(Connection& connection);
    };

    boost::asio::ip::tcp::acceptor acceptor_;
    std::unordered_map<std::string, PathContext> paths_;

    void async_accept();
};

}  // namespace rav::rtsp
