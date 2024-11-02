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

#include "rtsp_connection.hpp"
#include "rtsp_parser.hpp"
#include "ravennakit/containers/string_buffer.hpp"

#include <asio.hpp>

namespace rav {

class rtsp_client;

struct rtsp_connect_event {
    rtsp_client& client;
};

/**
 * Client for connecting to an RTSP server. Given io_context must be single-threaded to implicitly support
 * thread-safety.
 */
class rtsp_client final: public events<rtsp_connect_event, rtsp_response, rtsp_request>, public rtsp_connection {
  public:
    explicit rtsp_client(asio::io_context& io_context);

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

protected:
    void on_connected() override;
    void on_rtsp_request(const rtsp_request& request) override;
    void on_rtsp_response(const rtsp_response& response) override;

  private:
    asio::ip::tcp::resolver resolver_;
    std::string host_;

    void async_resolve_connect(const std::string& host, const std::string& service, asio::ip::resolver_base::flags flags);
};

}  // namespace rav
