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

#include "ravenna_rtsp_client.hpp"
#include "ravenna_sink.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"

#include <string>

namespace rav {

/**
 * Represents a RAVENNA node as specified in the RAVENNA operating principles document.
 */
class ravenna_node {
public:
    ravenna_node();
    void create_sink(const std::string& stream_name);

private:
    asio::io_context io_context_;
    std::unique_ptr<dnssd::dnssd_browser> node_browser_;
    std::unique_ptr<dnssd::dnssd_browser> session_browser_;
    // ravenna_rtsp_client ravenna_rtsp_client_{io_context_};
    std::vector<ravenna_sink> sinks_;
};

}
