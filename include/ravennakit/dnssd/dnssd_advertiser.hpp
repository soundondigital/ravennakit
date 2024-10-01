#pragma once

#include <utility>

#include "Util.h"
#include "service_description.hpp"
#include "ravennakit/core/event_emitter.hpp"

#include <functional>

namespace rav::dnssd {

namespace events {
    /**
     * Event for when a service was discovered.
     */
    struct advertiser_error {
        /// The exception
        const std::exception& exception;
    };
}  // namespace events

/**
 * Interface class which represents a dnssd advertiser object, which is able to present itself onto the network.
 */
class dnssd_advertiser: public event_emitter<dnssd_advertiser, events::advertiser_error> {
  public:
    explicit dnssd_advertiser() = default;
    ~dnssd_advertiser() override = default;

    /**
     * Registers a service with given arguments.
     *
     * @param reg_type The service type followed by the protocol, separated by a dot (e.g. "_ftp._tcp"). The service
     * type must be an underscore, followed by 1-15 characters, which may be letters, digits, or hyphens. The transport
     * protocol must be "_tcp" or "_udp".
     * @param name If non-NULL, specifies the service name to be registered. Most applications will not specify a name,
     * in which case the computer name is used (this name is communicated to the client via the callback).
     * @param domain If non-NULL, specifies the domain on which to advertise the service. Most applications will not
     * specify a domain, instead automatically registering in the default domain(s).
     * @param port The port of the service.
     * on the computer name which in most cases makes sense to do.
     * @param txt_record A TXT record to add to the service, consisting of a couple of keys and values.
     * @throws When an error occurs during registration.
     */
    virtual void register_service(
        const std::string& reg_type, const char* name, const char* domain, uint16_t port, const txt_record& txt_record
    ) = 0;

    /**
     * Updates the TXT record of this service. The given TXT record will replace the previous one.
     * @param txt_record The new TXT record.
     * @throws When an error occurs during updating.
     */
    virtual void update_txt_record(const txt_record& txt_record) = 0;

    /**
     * Unregisters this service from the mDnsResponder, after which the service will no longer be found on the network.
     */
    virtual void unregister_service() noexcept = 0;
};

}  // namespace rav::dnssd
