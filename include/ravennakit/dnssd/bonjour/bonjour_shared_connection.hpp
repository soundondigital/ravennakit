#pragma once

#include "bonjour.hpp"

#if RAV_HAS_APPLE_DNSSD

    #include "bonjour_scoped_dns_service_ref.hpp"

namespace rav::dnssd {

/**
 * Represents a shared connection to the mdns responder.
 */
class bonjour_shared_connection {
  public:
    /**
     * Constructor which will create a connection and store the DNSServiceRef under RAII fashion.
     */
    bonjour_shared_connection();

    /**
     * @return Returns the DNSServiceRef held by this instance. The DNSServiceRef will still be owned by this class.
     */
    [[nodiscard]] DNSServiceRef service_ref() const noexcept {
        return service_ref_.service_ref();
    }

    /**
     * Resets the DNSServiceRef to nullptr.
     */
    void reset() noexcept {
        service_ref_.reset();
    }

  private:
    bonjour_scoped_dns_service_ref service_ref_;
};

}  // namespace rav::dnssd

#endif
