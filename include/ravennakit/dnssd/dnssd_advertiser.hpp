#pragma once

#include <utility>
#include <memory>

#include "service_description.hpp"
#include "ravennakit/core/event_emitter.hpp"
#include "ravennakit/core/result.hpp"
#include "ravennakit/util/id.hpp"

#include <asio/io_context.hpp>

namespace rav::dnssd {

namespace events {
    /**
     * Event for when a service was discovered.
     */
    struct advertiser_error {
        const std::string& error_message;
    };

    /**
     * Event for when a DNS-SD service registration failed due to a name conflict.
     */
    struct name_conflict {
        const char* reg_type;
        const char* name;
    };
}  // namespace events

/**
 * Interface class which represents a dnssd advertiser object, which is able to present itself onto the network.
 */
class dnssd_advertiser: public event_emitter<dnssd_advertiser, events::advertiser_error, events::name_conflict> {
  public:
    explicit dnssd_advertiser() = default;
    ~dnssd_advertiser() override = default;

    /**
     * Registers a service with given arguments.
     * Function is not thread safe.
     *
     * @param reg_type The service type followed by the protocol, separated by a dot (e.g. "_ftp._tcp"). The service
     * type must be an underscore, followed by 1-15 characters, which may be letters, digits, or hyphens. The
     * transport protocol must be "_tcp" or "_udp".
     * @param name If non-NULL, specifies the service name to be registered. Most applications will not specify a
     * name, in which case the computer name is used (this name is communicated to the client via the callback).
     * @param domain If non-NULL, specifies the domain on which to advertise the service. Most applications will not
     * specify a domain, instead automatically registering in the default domain(s).
     * @param port The port of the service.
     * on the computer name which in most cases makes sense to do.
     * @param txt_record A TXT record to add to the service, consisting of a couple of keys and values.
     * @param auto_rename When true, the name will be automatically renamed if a conflict occurs. If false an
     * events::name_conflict will be emitted.
     * @throws When an error occurs during registration.
     */
    virtual util::id register_service(
        const std::string& reg_type, const char* name, const char* domain, uint16_t port, const txt_record& txt_record,
        bool auto_rename
    ) = 0;

    /**
     * Updates the TXT record of this service. The given TXT record will replace the previous one.
     * Function is not thread safe.
     *
     * @param id
     * @param txt_record The new TXT record.
     * @throws When an error occurs during updating.
     */
    virtual void update_txt_record(util::id id, const txt_record& txt_record) = 0;

    /**
     * Unregisters this service from the mDnsResponder, after which the service will no longer be found on the network.
     * Function is not thread safe.
     */
    virtual void unregister_service(util::id id) = 0;

    /**
     * Creates the most appropriate dnssd_advertiser implementation for the platform.
     * @return The created dnssd_advertiser instance, or nullptr if no implementation is available.
     */
    static std::unique_ptr<dnssd_advertiser> create(asio::io_context& io_context);
};

}  // namespace rav::dnssd
