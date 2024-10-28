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

#include "ravennakit/core/events.hpp"
#include "ravennakit/core/linked_node.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"
#include "ravennakit/rtsp/rtsp_client.hpp"

namespace rav {

struct ravenna_node_resolved {
    const dnssd::service_description& description;
};

struct ravenna_session_resolved {
    const dnssd::service_description& description;
};

/**
 * Discovers RAVENNA nodes and streams.
 */
class ravenna_browser final {
  public:
    using subscriber = linked_node<events<ravenna_node_resolved, ravenna_session_resolved>>;

    explicit ravenna_browser(asio::io_context& io_context);

    void subscribe(subscriber& s);

  private:
    asio::io_context& io_context_;
    std::unique_ptr<dnssd::dnssd_browser> node_browser_;
    std::unique_ptr<dnssd::dnssd_browser> session_browser_;
    subscriber subscribers_;
};

}  // namespace rav
