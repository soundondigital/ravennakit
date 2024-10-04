#pragma once

#include "bonjour.hpp"

#if RAV_HAS_APPLE_DNSSD

namespace rav::dnssd {

/**
 * RAII wrapper around DNSServiceRef.
 */
class bonjour_scoped_dns_service_ref {
  public:
    bonjour_scoped_dns_service_ref() = default;
    ~bonjour_scoped_dns_service_ref();

    explicit bonjour_scoped_dns_service_ref(const DNSServiceRef& service_ref) noexcept;

    bonjour_scoped_dns_service_ref(const bonjour_scoped_dns_service_ref&) = delete;
    bonjour_scoped_dns_service_ref& operator=(const bonjour_scoped_dns_service_ref& other) = delete;

    bonjour_scoped_dns_service_ref(bonjour_scoped_dns_service_ref&& other) noexcept;
    bonjour_scoped_dns_service_ref& operator=(bonjour_scoped_dns_service_ref&& other) noexcept;

    /**
     * Assigns an existing DNSServiceRef to this instance. An existing DNSServiceRef will be deallocated, and this
     * object will take ownership over the given DNSServiceRef.
     * @param service_ref The DNSServiceRef to assign to this instance.
     * @return A reference to this instance.
     */
    bonjour_scoped_dns_service_ref& operator=(DNSServiceRef service_ref);

    /**
     * @return Returns the contained DNSServiceRef.
     */
    [[nodiscard]] DNSServiceRef service_ref() const noexcept;

    /**
     * Resets the contained DNSServiceRef to nullptr.
     */
    void reset() noexcept;

  private:
    DNSServiceRef service_ref_ = nullptr;
};

}  // namespace rav::dnssd

#endif
