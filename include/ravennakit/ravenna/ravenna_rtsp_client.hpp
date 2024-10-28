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

#include "ravenna_browser.hpp"
#include "ravennakit/sdp/session_description.hpp"

namespace rav {

/**
 * Maintains connections to zero or more RAVENNA RTSP servers, upon request.
 */
class ravenna_rtsp_client {
  public:
    struct sdp_updated {
        const std::string& session_name;
        const sdp::session_description& sdp;
    };

    using subscriber = linked_node<events<sdp_updated>>;

    explicit ravenna_rtsp_client(asio::io_context& io_context, ravenna_browser& browser);

    void subscribe(const std::string& session_name, subscriber& s);

  private:
    asio::io_context& io_context_;
    ravenna_browser::subscriber ravenna_browser_subscriber_;

    enum class session_state { initial };

    struct session_context {
        subscriber subscribers;
        std::optional<sdp::session_description> sdp_;
        session_state state = session_state::initial;
    };

    std::map<std::string, session_context> sessions_;  // session_name -> session_context
    std::map<std::string, rtsp_client> connections_;   // connection_address -> rtsp_client
};

}  // namespace rav
