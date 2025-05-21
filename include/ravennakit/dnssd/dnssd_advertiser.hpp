#pragma once

#include <utility>
#include <memory>

#include "dnssd_service_description.hpp"
#include "../core/events/event_emitter.hpp"
#include "../core/util/linked_node.hpp"
#include "ravennakit/core/result.hpp"
#include "ravennakit/core/util/id.hpp"

#include <boost/asio/io_context.hpp>

namespace rav::dnssd {

/**
 * Base class for all dnssd advertiser implementations.
 */
class Advertiser {
  public:
    /**
     * Event for when a service was discovered.
     */
    struct AdvertiserError {
        const std::string& error_message;
    };

    /**
     * Event for when a DNS-SD service registration failed due to a name conflict.
     */
    struct NameConflict {
        const char* reg_type;
        const char* name;
    };

    using EventEmitterType = EventEmitter<AdvertiserError, NameConflict>;

    explicit Advertiser() = default;
    virtual ~Advertiser() = default;

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
     * @param local_only When true, service will only be advertised on the local machine.
     * @throws When an error occurs during registration.
     */
    virtual Id register_service(
        const std::string& reg_type, const char* name, const char* domain, uint16_t port, const TxtRecord& txt_record,
        bool auto_rename, bool local_only
    ) = 0;

    /**
     * Updates the TXT record of this service. The given TXT record will replace the previous one.
     * Function is not thread safe.
     *
     * Note: updating the TXT record will not trigger a callback for local_only services.
     *
     * @param id
     * @param txt_record The new TXT record.
     * @throws When an error occurs during updating.
     */
    virtual void update_txt_record(Id id, const TxtRecord& txt_record) = 0;

    /**
     * Unregisters this service from the mDnsResponder, after which the service will no longer be found on the network.
     * Function is not thread safe.
     */
    virtual void unregister_service(Id id) = 0;

    /**
     * Creates the most appropriate dnssd_advertiser implementation for the platform.
     * @return The created dnssd_advertiser instance, or nullptr if no implementation is available.
     */
    static std::unique_ptr<Advertiser> create(boost::asio::io_context& io_context);

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
