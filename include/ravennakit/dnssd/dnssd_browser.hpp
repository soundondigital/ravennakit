#pragma once

#include "service_description.hpp"
#include "ravennakit/core/event_emitter.hpp"

#include <functional>
#include <memory>

namespace rav::dnssd {

namespace events {

    /**
     * Event for when a service was discovered.
     * Note: this event will be emitted asynchronously from a background thread.
     */
    struct service_discovered {
        /// The service description of the discovered service.
        const service_description& description;
    };

    /**
     * Event for when a service was removed.
     * Note: this event will be emitted asynchronously from a background thread.
     */
    struct service_removed {
        /// The service description of the removed service.
        const service_description& description;
    };

    /**
     * Event for when a service was resolved (i.e. address information was resolved).
     * Note: this event will be emitted asynchronously from a background thread.
     */
    struct service_resolved {
        /// The service description of the resolved service.
        const service_description& description;
        /// The index of the interface on which the service was resolved on.
        uint32_t interfaceIndex;
    };

    /**
     * Event for when the service became available on given address.
     * Note: this event will be emitted asynchronously from a background thread.
     */
    struct address_added {
        /// The service description of the service for which the address was added.
        const service_description& description;
        /// The address which was added.
        const std::string& address;
        /// The index of the interface on which the address was added.
        uint32_t interfaceIndex;
    };

    /**
     * Event for when the service became unavailable on given address.
     * Note: this event will be emitted asynchronously from a background thread.
     */
    struct address_removed {
        /// The service description of the service for which the address was removed.
        const service_description& description;
        /// The address which was removed.
        const std::string& address;
        /// The index of the interface on which the address was removed.
        uint32_t interfaceIndex;
    };

    /**
     * Event for when an error occurred during browsing for a service.
     * Note: this event might be emitted asynchronously from a background thread.
     */
    struct browse_error {
        const std::string& error_message;
    };
}  // namespace events

/**
 * Interface class which represents a Bonjour browser.
 */
class dnssd_browser:
    public event_emitter<
        dnssd_browser, events::service_discovered, events::service_removed, events::service_resolved,
        events::address_added, events::address_removed, events::browse_error> {
  public:
    ~dnssd_browser() override = default;

    /**
     * Starts browsing for a service
     * @param service_type The service type (i.e. _http._tcp.).
     * @return Returns a result indicating success or failure.
     */
    virtual void browse_for(const std::string& service_type) = 0;

    /**
     * Creates the most appropriate dnssd_browser implementation for the platform.
     * @return The created dnssd_browser instance, or nullptr if no implementation is available.
     */
    static std::unique_ptr<dnssd_browser> create();
};

}  // namespace rav::dnssd
