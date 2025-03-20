#include "ravennakit/core/rollback.hpp"
#include "ravennakit/dnssd/bonjour/bonjour.hpp"

#include <asio.hpp>

#if RAV_HAS_APPLE_DNSSD

    #include "ravennakit/dnssd/bonjour/bonjour_advertiser.hpp"
    #include "ravennakit/dnssd/dnssd_service_description.hpp"
    #include "ravennakit/dnssd/bonjour/bonjour_txt_record.hpp"

    #include <iostream>
    #include <thread>

rav::dnssd::BonjourAdvertiser::BonjourAdvertiser(asio::io_context& io_context) : service_socket_(io_context) {
    const int service_fd = DNSServiceRefSockFD(shared_connection_.service_ref());

    if (service_fd < 0) {
        RAV_THROW_EXCEPTION("Invalid file descriptor");
    }

    service_socket_.assign(asio::ip::tcp::v6(), service_fd);  // Not sure about v6
    async_process_results();
}

rav::id rav::dnssd::BonjourAdvertiser::register_service(
    const std::string& reg_type, const char* name, const char* domain, uint16_t port, const TxtRecord& txt_record,
    const bool auto_rename, const bool local_only
) {
    RAV_ASSERT(!reg_type.empty(), "Service type must not be empty");
    RAV_ASSERT(port != 0, "Port must not be 0");

    DNSServiceRef service_ref = shared_connection_.service_ref();
    const auto record = BonjourTxtRecord(txt_record);

    DNSServiceFlags flags = kDNSServiceFlagsShareConnection;

    if (!auto_rename) {
        flags |= kDNSServiceFlagsNoAutoRename;
    }

    if (local_only) {
        flags |= kDNSServiceInterfaceIndexLocalOnly;
    }

    const auto result = DNSServiceRegister(
        &service_ref, flags, 0, name, reg_type.c_str(), domain, nullptr, htons(port), record.length(),
        record.bytes_ptr(), register_service_callback, this
    );

    DNSSD_THROW_IF_ERROR(result, "Failed to register service");

    auto scoped_service_ref = BonjourScopedDnsServiceRef(service_ref);
    const auto id = id_generator_.next();
    registered_services_.push_back(registered_service {id, std::move(scoped_service_ref)});
    return id;
}

void rav::dnssd::BonjourAdvertiser::unregister_service(id id) {
    registered_services_.erase(
        std::remove_if(
            registered_services_.begin(), registered_services_.end(),
            [id](const registered_service& s) {
                return s.id == id;
            }
        ),
        registered_services_.end()
    );
}

void rav::dnssd::BonjourAdvertiser::subscribe(Subscriber& s) {
    subscribers_.push_back(s);
}

void rav::dnssd::BonjourAdvertiser::async_process_results() {
    service_socket_.async_wait(asio::ip::tcp::socket::wait_read, [this](const asio::error_code& ec) {
        if (ec) {
            if (ec != asio::error::operation_aborted) {
                RAV_ERROR("Error in async_wait_for_results: {}", ec.message());
            }
            return;
        }

        const auto result = DNSServiceProcessResult(shared_connection_.service_ref());

        if (result != kDNSServiceErr_NoError) {
            RAV_ERROR("DNSServiceError: {}", dns_service_error_to_string(result));
            emit(AdvertiserError {fmt::format("Process result error: {}", dns_service_error_to_string(result))});
            if (++process_results_failed_attempts_ > 10) {
                RAV_ERROR("Too many failed attempts to process results, stopping");
                emit(AdvertiserError {"Too many failed attempts to process results, stopping"});
                return;
            }
        } else {
            process_results_failed_attempts_ = 0;
        }

        async_process_results();
    });
}

void rav::dnssd::BonjourAdvertiser::register_service_callback(
    [[maybe_unused]] DNSServiceRef service_ref, [[maybe_unused]] const DNSServiceFlags flags,
    const DNSServiceErrorType error_code, [[maybe_unused]] const char* service_name,
    [[maybe_unused]] const char* reg_type, [[maybe_unused]] const char* reply_domain, [[maybe_unused]] void* context
) {
    RAV_ASSERT(context != nullptr, "Expected non-null context");

    auto* advertiser = static_cast<BonjourAdvertiser*>(context);
    if (error_code == kDNSServiceErr_NameConflict) {
        advertiser->emit(NameConflict {reg_type, service_name});
        return;
    }

    if (error_code != kDNSServiceErr_NoError) {
        advertiser->emit(AdvertiserError {fmt::format("Failed to register service: {}", error_code)});
        return;
    }
}

rav::dnssd::BonjourAdvertiser::registered_service*
rav::dnssd::BonjourAdvertiser::find_registered_service(const id id) {
    for (auto& service : registered_services_) {
        if (service.id == id) {
            return &service;
        }
    }

    return nullptr;
}

void rav::dnssd::BonjourAdvertiser::update_txt_record(const id id, const TxtRecord& txt_record) {
    const auto* const service = find_registered_service(id);
    if (service == nullptr) {
        RAV_THROW_EXCEPTION("Service not found");
    }

    auto const record = BonjourTxtRecord(txt_record);

    // Second argument's nullptr tells us that we are updating the primary record.
    const auto result =
        DNSServiceUpdateRecord(service->service_ref.service_ref(), nullptr, 0, record.length(), record.bytes_ptr(), 0);

    DNSSD_THROW_IF_ERROR(result, "Failed to update TXT record");
}

#endif
