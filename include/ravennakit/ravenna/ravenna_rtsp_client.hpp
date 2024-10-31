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
#include "ravennakit/sdp/session_description.hpp"

namespace rav {

/**
 * Maintains connections to zero or more RAVENNA RTSP servers, upon request.
 */
class ravenna_rtsp_client {
  public:
    struct announced {
        const std::string& session_name;
        const sdp::session_description& sdp;
    };

    class subscriber final: public events<announced> {
    public:
        subscriber() = default;
        ~subscriber() override;

        /**
         * Subscribes this subscriber to the ravenna_rtsp_client.
         * @param client The ravenna_rtsp_client to subscribe to.
         * @param session_name The name of the session to subscribe to.
         */
        void subscribe(ravenna_rtsp_client& client, const std::string& session_name);

        /**
         * Unsubscribes this subscriber from the ravenna_rtsp_client.
         * @param schedule_maintenance If true, maintenance will be scheduled after unsubscribing.
         */
        void unsubscribe(bool schedule_maintenance = true);

        /**
         * Unsubscribes this subscriber from the ravenna_rtsp_client, but without triggering maintenance.
         */
        void release();

      private:
        ravenna_rtsp_client* owner_{};
        linked_node<subscriber*> node_;
    };

    explicit ravenna_rtsp_client(asio::io_context& io_context, dnssd::dnssd_browser& browser);
    ~ravenna_rtsp_client();

  private:
    struct session_context {
        std::string session_name;
        linked_node<subscriber*> subscribers;
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
};

}  // namespace rav
