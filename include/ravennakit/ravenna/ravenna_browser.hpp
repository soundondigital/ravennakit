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

#include "ravennakit/dnssd/dnssd_browser.hpp"
#include "ravennakit/rtsp/rtsp_client.hpp"

#include <asio/io_context.hpp>

namespace rav {

/**
 * Discovers RAVENNA nodes and streams on the network and manages RTSP connections to them.
 */
class ravenna_browser {
  public:
    explicit ravenna_browser(asio::io_context& io_context);

  private:
    std::unique_ptr<dnssd::dnssd_browser> node_browser_;
    std::unique_ptr<dnssd::dnssd_browser> session_browser_;
};

}  // namespace rav
