#pragma once

#include "dnssd_service_description.hpp"
#include "../core/events/event_emitter.hpp"
#include "ravennakit/core/util/linked_node.hpp"

#include <boost/asio.hpp>

#include <functional>
#include <memory>

namespace rav::dnssd {

/**
 * Interface class which represents a Bonjour browser.
 */
class Browser {
  public:
    /**
     * Event for when a service was discovered.
     * Note: this event will be emitted asynchronously from a background thread.
     */
    struct ServiceDiscovered {
        /// The service description of the discovered service.
        const ServiceDescription& description;
    };

    /**
     * Event for when a service was removed.
     * Note: this event will be emitted asynchronously from a background thread.
     */
    struct ServiceRemoved {
        /// The service description of the removed service.
        const ServiceDescription& description;
    };

    /**
     * Event for when a service was resolved (i.e. address information was resolved).
     * Note: this event will be emitted asynchronously from a background thread.
     */
    struct ServiceResolved {
        /// The service description of the resolved service.
        const ServiceDescription& description;
    };

    /**
     * Event for when the service became available on given address.
     * Note: this event will be emitted asynchronously from a background thread.
     */
    struct AddressAdded {
        /// The service description of the service for which the address was added.
        const ServiceDescription& description;
        /// The address which was added.
        const std::string& address;
        /// The index of the interface on which the address was added.
        uint32_t interface_index;
    };

    /**
     * Event for when the service became unavailable on given address.
     * Note: this event will be emitted asynchronously from a background thread.
     */
    struct AddressRemoved {
        /// The service description of the service for which the address was removed.
        const ServiceDescription& description;
        /// The address which was removed.
        const std::string& address;
        /// The index of the interface on which the address was removed.
        uint32_t interface_index;
    };

    /**
     * Event for when an error occurred during browsing for a service.
     * Note: this event might be emitted asynchronously from a background thread.
     */
    struct BrowseError {
        const std::string& error_message;
    };

    using EventEmitterType =
        EventEmitter<ServiceDiscovered, ServiceRemoved, ServiceResolved, AddressAdded, AddressRemoved, BrowseError>;

    virtual ~Browser() = default;

    /**
     * Starts browsing for a service.
     * This function is not thread safe.
     * @param reg_type The service type (i.e. _http._tcp.).
     * @return Returns a result indicating success or failure.
     */
    virtual void browse_for(const std::string& reg_type) = 0;

    /**
     * Creates the most appropriate dnssd_browser implementation for the platform.
     * @return The created dnssd_browser instance, or nullptr if no implementation is available.
     */
    static std::unique_ptr<Browser> create(boost::asio::io_context& io_context);

    /**
     * Tries to find a service by its name.
     * @param service_name The name of the service to find.
     * @return The service description if found, otherwise nullptr.
     */
    [[nodiscard]] virtual const ServiceDescription* find_service(const std::string& service_name) const = 0;

    /**
     * @returns A list of existing services.
     */
    [[nodiscard]] virtual std::vector<ServiceDescription> get_services() const = 0;

    /**
     * Sets given function as callback for the given event.
     * @tparam Fn The type of the function to be called.
     * @param f The function to be called when the event occurs.
     */
    template<typename Fn>
    void on(EventEmitterType::handler<Fn> f) {
        event_emitter_.on(f);
    }

  protected:
    EventEmitterType event_emitter_;
};

}  // namespace rav::dnssd
