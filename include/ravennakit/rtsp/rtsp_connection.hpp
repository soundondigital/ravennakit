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
#include "ravennakit/containers/string_buffer.hpp"

#include <asio.hpp>

namespace rav {

class rtsp_connection {
public:
    virtual ~rtsp_connection();
    explicit rtsp_connection(asio::ip::tcp::socket socket);

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

  protected:
    void async_connect(const asio::ip::tcp::resolver::results_type& results);
    void async_send_data(const std::string& data);
    void async_write();
    void async_read_some();

    virtual void on_connected() {}
    virtual void on_rtsp_request([[maybe_unused]] const rtsp_request& request) {}
    virtual void on_rtsp_response([[maybe_unused]] const rtsp_response& response) {}

private:
    asio::ip::tcp::socket socket_;
    string_buffer input_buffer_;
    string_buffer output_buffer_;
    rtsp_parser parser_;
};

}
