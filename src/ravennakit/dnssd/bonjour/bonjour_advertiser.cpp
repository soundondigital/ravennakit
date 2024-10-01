#include "ravennakit/core/rollback.hpp"
#include "ravennakit/dnssd/bonjour/bonjour.hpp"

#if RAV_HAS_APPLE_DNSSD

    #include "ravennakit/dnssd/bonjour/bonjour_advertiser.hpp"
    #include "ravennakit/dnssd/service_description.hpp"
    #include "ravennakit/dnssd/bonjour/bonjour_txt_record.hpp"

    #include <iostream>
    #include <thread>

void rav::dnssd::bonjour_advertiser::register_service(
    const std::string& reg_type, const char* name, const char* domain, uint16_t port, const txt_record& txt_record
) {
    DNSServiceRef service_ref = nullptr;
    const auto record = bonjour_txt_record(txt_record);

    DNSSD_THROW_IF_ERROR(DNSServiceRegister(
        &service_ref, 0, 0, name, reg_type.c_str(), domain, nullptr, htons(port), record.length(), record.bytesPtr(),
        register_service_callback, this
    ));

    rollback rollback([this]() {
        unregister_service();
    });

    service_ref_ = service_ref;
    DNSSD_THROW_IF_ERROR(DNSServiceProcessResult(service_ref_.service_ref()));

    rollback.commit();
}

void rav::dnssd::bonjour_advertiser::unregister_service() noexcept {
    service_ref_.reset();
}

void rav::dnssd::bonjour_advertiser::register_service_callback(
    [[maybe_unused]] DNSServiceRef service_ref, [[maybe_unused]] const DNSServiceFlags flags,
    const DNSServiceErrorType error_code, [[maybe_unused]] const char* service_name,
    [[maybe_unused]] const char* reg_type, [[maybe_unused]] const char* reply_domain, [[maybe_unused]] void* context
) {
    DNSSD_THROW_IF_ERROR(error_code);
}

void rav::dnssd::bonjour_advertiser::update_txt_record(const txt_record& txt_record) {
    auto const record = bonjour_txt_record(txt_record);

    // Second argument's nullptr tells us that we are updating the primary record.
    DNSSD_THROW_IF_ERROR(
        DNSServiceUpdateRecord(service_ref_.service_ref(), nullptr, 0, record.length(), record.bytesPtr(), 0)
    );
}

#endif
