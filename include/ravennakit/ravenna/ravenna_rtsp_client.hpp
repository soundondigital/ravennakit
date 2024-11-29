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
#include "ravennakit/sdp/sdp_session_description.hpp"

namespace rav {

/**
 * Maintains connections to zero or more RAVENNA RTSP servers, upon request.
 */
class ravenna_rtsp_client {
  public:
    struct announced_event {
        const std::string& session_name;
        const sdp::session_description& sdp;
    };

    class subscriber {
      public:
        subscriber() = default;
        virtual ~subscriber();

        subscriber(const subscriber&) = delete;
        subscriber& operator=(const subscriber&) = delete;

        subscriber(subscriber&&) noexcept = delete;
        subscriber& operator=(subscriber&&) noexcept = delete;

        /**
         * Called when a session is announced.
         * @param event The announced event.
         */
        virtual void on_announced([[maybe_unused]] const announced_event& event) {}

      protected:
        /**
         * Subscribes this subscriber to the ravenna_rtsp_client.
         * @param client The ravenna_rtsp_client to subscribe to.
         * @param session_name The name of the session to subscribe to.
         */
        void subscribe_to_ravenna_rtsp_client(ravenna_rtsp_client& client, const std::string& session_name);

        /**
         * Unsubscribes this subscriber from the ravenna_rtsp_client.
         */
        void unsubscribe_from_ravenna_rtsp_client();

      private:
        linked_node<std::pair<subscriber*, ravenna_rtsp_client*>> node_;
    };

    explicit ravenna_rtsp_client(asio::io_context& io_context, dnssd::dnssd_browser& browser);
    ~ravenna_rtsp_client();

  private:
    struct session_context {
        std::string session_name;
        linked_node<std::pair<subscriber*, ravenna_rtsp_client*>> subscribers;
        std::optional<sdp::session_description> sdp_;
        std::string host_target;
        uint16_t port {};
    };

    struct connection_context {
        std::string host_target;
        uint16_t port {};
        rtsp_client client;
    };

    asio::io_context& io_context_;
    dnssd::dnssd_browser& browser_;
    dnssd::dnssd_browser::subscriber browser_subscriber_;
    std::vector<session_context> sessions_;
    std::vector<connection_context> connections_;

    connection_context& find_or_create_connection(const std::string& host_target, uint16_t port);
    connection_context* find_connection(const std::string& host_target, uint16_t port);
    void update_session_with_service(session_context& session, const dnssd::service_description& service);
    void do_maintenance();
    void handle_incoming_sdp(const std::string& sdp_text);
};

}  // namespace rav
