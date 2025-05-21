/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/core/events/subscriber_list.hpp"
#include "ravennakit/dnssd/dnssd_browser.hpp"

namespace rav {

/**
 * Convenience class which contains a dnssd browser for nodes and sessions.
 */
class RavennaBrowser {
  public:
    /**
     * Baseclass for other classes which need updates on discovered nodes and sessions.
     */
    class Subscriber {
      public:
        virtual ~Subscriber() = default;

        /**
         * Called when a node is discovered.
         * @param event The event containing the service description.
         */
        virtual void ravenna_node_discovered([[maybe_unused]] const dnssd::Browser::ServiceResolved& event) {}

        /**
         * Called when a node is removed.
         * @param event The event containing the service description.
         */
        virtual void ravenna_node_removed([[maybe_unused]] const dnssd::Browser::ServiceRemoved& event) {}

        /**
         * Called when a session is discovered.
         * @param event The event containing the service description.
         */
        virtual void ravenna_session_discovered([[maybe_unused]] const dnssd::Browser::ServiceResolved& event) {}

        /**
         * Called when a session is removed.
         * @param event The event containing the service description.
         */
        virtual void ravenna_session_removed([[maybe_unused]] const dnssd::Browser::ServiceRemoved& event) {}
    };

    explicit RavennaBrowser(boost::asio::io_context& io_context);

    /**
     * Finds a node by its name.
     * @param session_name The name of the node to find.
     * @return The service description of the node, or nullptr if not found.
     */
    [[nodiscard]] const dnssd::ServiceDescription* find_session(const std::string& session_name) const;

    /**
     * Finds a node by its name.
     * @param node_name The name of the node to find.
     * @return The service description of the node, or nullptr if not found.
     */
    [[nodiscard]] const dnssd::ServiceDescription* find_node(const std::string& node_name) const;

    /**
     * Adds a subscriber to the browser.
     * @param subscriber_to_add The subscriber to add.
     * @return true if the subscriber was added, or false if it was already in the list.
     */
    [[nodiscard]] bool subscribe(Subscriber* subscriber_to_add);

    /**
     * Removes a subscriber from the browser.
     * @param subscriber_to_remove The subscriber to remove.
     * @return true if the subscriber was removed, or false if it wasn't found.
     */
    [[nodiscard]] bool unsubscribe(Subscriber* subscriber_to_remove);

  private:
    std::unique_ptr<dnssd::Browser> node_browser_;
    std::unique_ptr<dnssd::Browser> session_browser_;

    SubscriberList<Subscriber> subscribers_;
};

}  // namespace rav
