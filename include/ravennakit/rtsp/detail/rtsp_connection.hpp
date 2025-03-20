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

#include "rtsp_parser.hpp"
#include "ravennakit/core/containers/string_buffer.hpp"

#include <asio.hpp>

namespace rav::rtsp {

class Connection final: public std::enable_shared_from_this<Connection> {
  public:
    struct ConnectEvent {
        Connection& rtsp_connection;
    };

    struct RequestEvent {
        Connection& rtsp_connection;
        const Request& rtsp_request;
    };

    struct ResponseEvent {
        Connection& rtsp_connection;
        const Response& rtsp_response;
    };

    /**
     * Observer for the connection.
     */
    class Subscriber {
      public:
        Subscriber() = default;
        virtual ~Subscriber() = default;

        Subscriber(const Subscriber&) = default;
        Subscriber& operator=(const Subscriber&) = default;

        Subscriber(Subscriber&&) noexcept = default;
        Subscriber& operator=(Subscriber&&) noexcept = default;

        /**
         * Called when a connection is established.
         * @param connection The connection that was established.
         */
        virtual void on_connect(Connection& connection) {
            std::ignore = connection;
        }

        /**
         * Called when a request is received.
         * @param connection The connection on which the request was received.
         * @param request The request that was received.
         */
        virtual void on_request(Connection& connection, const Request& request) {
            std::ignore = connection;
            std::ignore = request;
        }

        /**
         * Called when a response is received.
         * @param connection The connection on which the response was received.
         * @param response The response that was received.
         */
        virtual void on_response(Connection& connection, const Response& response) {
            std::ignore = connection;
            std::ignore = response;
        }

        /**
         * Called when a connection is disconnected.
         * @param connection The connection that was disconnected.
         */
        virtual void on_disconnect(Connection& connection) {
            std::ignore = connection;
        }
    };

    static std::shared_ptr<Connection> create(asio::ip::tcp::socket socket) {
        return std::shared_ptr<Connection>(new Connection(std::move(socket)));
    }

    ~Connection();

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&&) noexcept = default;
    Connection& operator=(Connection&&) noexcept = default;

    /**
     * Sends given response to the server. Function is async and will return immediately.
     * @param response The response to send.
     */
    void async_send_response(const Response& response);

    /**
     * Sends given request to the server. Function is async and will return immediately.
     * @param request The request to send.
     */
    void async_send_request(const Request& request);

    /**
     * Shuts down the connection for both directions.
     */
    void shutdown();

    /**
     * Starts the connection by reading from the socket.
     */
    void start();

    /**
     * Stops the connection by closing the socket.
     */
    void stop();

    /**
     * Sets the subscriber for this connection.
     */
    void set_subscriber(Subscriber* subscriber_to_set);

    /**
     * Connects to the given host and port.
     */
    void async_connect(const asio::ip::tcp::resolver::results_type& results);

    /**
     * Sends data to the server. Function is async and will return immediately.
     * @param data The data to send. The data is expected to be properly formatted as RTSP request or response.
     */
    void async_send_data(const std::string& data);

    /**
     * @return The remote endpoint of the connection.
     * @throws If the connection is not established.
     */
    asio::ip::tcp::endpoint remote_endpoint() const;

  private:
    asio::ip::tcp::socket socket_;
    string_buffer input_buffer_;
    string_buffer output_buffer_;
    Parser parser_;
    Subscriber* subscriber_ {};

    explicit Connection(asio::ip::tcp::socket socket);

    void async_write();
    void async_read_some();
};

}  // namespace rav::rtsp
