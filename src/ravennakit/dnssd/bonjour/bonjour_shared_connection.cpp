#include "ravennakit/dnssd/bonjour/bonjour.hpp"

#if RAV_HAS_APPLE_DNSSD

    #include "ravennakit/dnssd/bonjour/bonjour_shared_connection.hpp"
    #include "ravennakit/core/log.hpp"

rav::dnssd::bonjour_shared_connection::bonjour_shared_connection() {
    DNSServiceRef ref = nullptr;
    DNSSD_THROW_IF_ERROR(DNSServiceCreateConnection(&ref), "Failed to create shared connection");
    service_ref_ = ref;  // From here on the ref is under RAII inside a ScopedDnsServiceRef class
}

#endif
