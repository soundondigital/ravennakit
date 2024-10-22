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

namespace rtsp {
    struct connect_event {};
}  // namespace rtsp

class rtsp_client final: public event_emitter<rtsp_client, rtsp::connect_event, rtsp_response, rtsp_request> {
  public:
    explicit rtsp_client(asio::io_context& io_context);

    /**
     * Connect to the given endpoint. Function is async and will return immediately.
     * @param endpoint The endpoint to connect to.
     */
    void connect(const asio::ip::tcp::endpoint& endpoint);

    /**
     * Connect to the given address and port. Function is async and will return immediately.
     * @param addr The address to connect to.
     * @param port The port to connect to.
     */
    void connect(const std::string& addr, uint16_t port);

    /**
     * Send a DESCRIBE request to the server. Function is async and will return immediately.
     * @param path The path to describe
     */
    void describe(const std::string& path);

    /**
     * Sends a SETUP request to the server. Function is async and will return immediately.
     * @param path The path to setup.
     */
    void setup(const std::string& path);

    /**
     * Sends a PLAY request to the server. Function is async and will return immediately.
     * @param path The path for the PLAY command.
     */
    void play(const std::string& path);

    /**
     * Post some work through the executor of the socket.
     * @param work The work to execute.
     */
    void post(std::function<void()> work);

  private:
    asio::ip::tcp::socket socket_;
    string_buffer input_buffer_;
    string_buffer output_buffer_;
    rtsp_parser parser_;

    void async_connect(const asio::ip::tcp::endpoint& endpoint);
    void async_write();
    void async_read_some();
};

}  // namespace rav
