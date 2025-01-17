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

namespace rav {

class rtsp_connection final: public std::enable_shared_from_this<rtsp_connection> {
  public:
    struct connect_event {
        rtsp_connection& connection;
    };

    struct request_event {
        rtsp_connection& connection;
        const rtsp_request& request;
    };

    struct response_event {
        rtsp_connection& connection;
        const rtsp_response& response;
    };

    /**
     * Observer for the connection.
     */
    class subscriber {
      public:
        subscriber() = default;
        virtual ~subscriber() = default;

        subscriber(const subscriber&) = default;
        subscriber& operator=(const subscriber&) = default;

        subscriber(subscriber&&) noexcept = default;
        subscriber& operator=(subscriber&&) noexcept = default;

        /**
         * Called when a connection is established.
         * @param connection The connection that was established.
         */
        virtual void on_connect(rtsp_connection& connection) {
            std::ignore = connection;
        }

        /**
         * Called when a request is received.
         * @param connection The connection on which the request was received.
         * @param request The request that was received.
         */
        virtual void on_request(rtsp_connection& connection, const rtsp_request& request) {
            std::ignore = connection;
            std::ignore = request;
        }

        /**
         * Called when a response is received.
         * @param connection The connection on which the response was received.
         * @param response The response that was received.
         */
        virtual void on_response(rtsp_connection& connection, const rtsp_response& response) {
            std::ignore = connection;
            std::ignore = response;
        }

        /**
         * Called when a connection is disconnected.
         * @param connection The connection that was disconnected.
         */
        virtual void on_disconnect(rtsp_connection& connection) {
            std::ignore = connection;
        }
    };

    static std::shared_ptr<rtsp_connection> create(asio::ip::tcp::socket socket) {
        return std::shared_ptr<rtsp_connection>(new rtsp_connection(std::move(socket)));
    }

    ~rtsp_connection();

    rtsp_connection(const rtsp_connection&) = delete;
    rtsp_connection& operator=(const rtsp_connection&) = delete;

    rtsp_connection(rtsp_connection&&) noexcept = default;
    rtsp_connection& operator=(rtsp_connection&&) noexcept = default;

    /**
     * Sends given response to the server. Function is async and will return immediately.
     * @param response The response to send.
     */
    void async_send_response(const rtsp_response& response);

    /**
     * Sends given request to the server. Function is async and will return immediately.
     * @param request The request to send.
     */
    void async_send_request(const rtsp_request& request);

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
    void set_subscriber(subscriber* subscriber_to_set);

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
    rtsp_parser parser_;
    subscriber* subscriber_ {};

    explicit rtsp_connection(asio::ip::tcp::socket socket);

    void async_write();
    void async_read_some();
};

}  // namespace rav
