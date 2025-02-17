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
#include "ravenna_receiver.hpp"
#include "ravennakit/core/events/event_emitter.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"

#include <string>

namespace rav {

/**
 * Represents a RAVENNA node as specified in the RAVENNA operating principles document.
 */
class ravenna_node {
  public:
    class subscriber {
      public:
        virtual ~subscriber() = default;

        virtual void ravenna_node_discovered([[maybe_unused]] const dnssd::dnssd_browser::service_resolved& event) {}

        virtual void ravenna_node_removed([[maybe_unused]] const dnssd::dnssd_browser::service_removed& event) {}

        virtual void ravenna_session_discovered([[maybe_unused]] const dnssd::dnssd_browser::service_resolved& event) {}

        virtual void ravenna_session_removed([[maybe_unused]] const dnssd::dnssd_browser::service_removed& event) {}
    };

    ravenna_node();
    ~ravenna_node();

    void create_receiver(const std::string& ravenna_session_name);

    bool subscribe(subscriber* s);
    bool unsubscribe(subscriber* s);

  private:
    asio::io_context io_context_;
    std::thread maintenance_thread_;

    std::unique_ptr<dnssd::dnssd_browser> node_browser_;
    dnssd::dnssd_browser::subscriber node_browser_subscriber_;

    std::unique_ptr<dnssd::dnssd_browser> session_browser_;
    dnssd::dnssd_browser::subscriber session_browser_subscriber_;

    std::vector<ravenna_receiver> receivers_;
    subscriber_list<subscriber> subscribers_;
};

}  // namespace rav
