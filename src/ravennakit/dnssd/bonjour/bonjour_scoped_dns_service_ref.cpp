#include "ravennakit/dnssd/bonjour/bonjour.hpp"

#if RAV_HAS_APPLE_DNSSD

    #include "ravennakit/dnssd/bonjour/bonjour_scoped_dns_service_ref.hpp"

    #include <utility>

rav::dnssd::bonjour_scoped_dns_service_ref::~bonjour_scoped_dns_service_ref() {
    reset();
}

rav::dnssd::bonjour_scoped_dns_service_ref::bonjour_scoped_dns_service_ref(bonjour_scoped_dns_service_ref&& other
) noexcept {
    *this = std::move(other);
}

rav::dnssd::bonjour_scoped_dns_service_ref::bonjour_scoped_dns_service_ref(const DNSServiceRef& service_ref) noexcept :
    service_ref_(service_ref) {}

rav::dnssd::bonjour_scoped_dns_service_ref&
rav::dnssd::bonjour_scoped_dns_service_ref::operator=(bonjour_scoped_dns_service_ref&& other) noexcept {
    reset();
    service_ref_ = other.service_ref_;
    other.service_ref_ = nullptr;
    return *this;
}

rav::dnssd::bonjour_scoped_dns_service_ref&
rav::dnssd::bonjour_scoped_dns_service_ref::operator=(DNSServiceRef service_ref) {
    reset();
    service_ref_ = service_ref;
    return *this;
}

DNSServiceRef rav::dnssd::bonjour_scoped_dns_service_ref::service_ref() const noexcept {
    return service_ref_;
}

void rav::dnssd::bonjour_scoped_dns_service_ref::reset() noexcept {
    if (service_ref_ != nullptr) {
        DNSServiceRefDeallocate(service_ref_);
        service_ref_ = nullptr;
    }
}

#endif
