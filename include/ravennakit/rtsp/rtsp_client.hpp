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

struct rtsp_connect_event {};

/**
 * Client for connecting to an RTSP server. Given io_context must be single-threaded to implicitly support
 * thread-safety.
 */
class rtsp_client final: public events<rtsp_connect_event, rtsp_response, rtsp_request> {
  public:
    explicit rtsp_client(asio::io_context& io_context);

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
     */
    void async_describe(const std::string& path);

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
     * Post some work through the executor of the socket.
     * @param work The work to execute.
     */
    void post(std::function<void()> work);

  private:
    asio::ip::tcp::resolver resolver_;
    asio::ip::tcp::socket socket_;
    std::string host_;
    string_buffer input_buffer_;
    string_buffer output_buffer_;
    rtsp_parser parser_;

    void async_connect(const std::string& host, const std::string& service, asio::ip::resolver_base::flags flags);
    void async_write();
    void async_read_some();
};

}  // namespace rav
