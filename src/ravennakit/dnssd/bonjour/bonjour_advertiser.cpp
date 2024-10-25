#include "ravennakit/core/rollback.hpp"
#include "ravennakit/dnssd/bonjour/bonjour.hpp"

#include <asio/dispatch.hpp>

#if RAV_HAS_APPLE_DNSSD

    #include "ravennakit/dnssd/bonjour/bonjour_advertiser.hpp"
    #include "ravennakit/dnssd/service_description.hpp"
    #include "ravennakit/dnssd/bonjour/bonjour_txt_record.hpp"

    #include <iostream>
    #include <thread>

rav::dnssd::bonjour_advertiser::bonjour_advertiser(asio::io_context& io_context) : service_socket_(io_context) {
    const int service_fd = DNSServiceRefSockFD(shared_connection_.service_ref());

    if (service_fd < 0) {
        RAV_THROW_EXCEPTION("Invalid file descriptor");
    }

    service_socket_.assign(asio::ip::tcp::v6(), service_fd);  // Not sure about v6
    async_process_results();
}

rav::dnssd::bonjour_advertiser::~bonjour_advertiser() {
    service_socket_.cancel();
    service_socket_.release();  // Release the socket to avoid closing it in the destructor
}

rav::util::id rav::dnssd::bonjour_advertiser::register_service(
    const std::string& reg_type, const char* name, const char* domain, uint16_t port, const txt_record& txt_record,
    const bool auto_rename
) {
    DNSServiceRef service_ref = shared_connection_.service_ref();
    const auto record = bonjour_txt_record(txt_record);

    DNSServiceFlags flags = kDNSServiceFlagsShareConnection;

    if (!auto_rename) {
        flags |= kDNSServiceFlagsNoAutoRename;
    }

    const auto result = DNSServiceRegister(
        &service_ref, flags, 0, name, reg_type.c_str(), domain, nullptr, htons(port), record.length(),
        record.bytesPtr(), register_service_callback, this
    );

    DNSSD_THROW_IF_ERROR(result, "Failed to register service");

    auto scoped_service_ref = bonjour_scoped_dns_service_ref(service_ref);
    const auto id = id_generator_.next();
    registered_services_.push_back(registered_service {id, std::move(scoped_service_ref)});
    return id;
}

void rav::dnssd::bonjour_advertiser::unregister_service(util::id id) {
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

void rav::dnssd::bonjour_advertiser::async_process_results() {
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
            emit(events::advertiser_error {fmt::format("Process result error: {}", dns_service_error_to_string(result))}
            );
            if (++process_results_failed_attempts_ > 10) {
                RAV_ERROR("Too many failed attempts to process results, stopping");
                emit(events::advertiser_error {"Too many failed attempts to process results, stopping"});
                return;
            }
        } else {
            process_results_failed_attempts_ = 0;
        }

        async_process_results();
    });
}

void rav::dnssd::bonjour_advertiser::register_service_callback(
    [[maybe_unused]] DNSServiceRef service_ref, [[maybe_unused]] const DNSServiceFlags flags,
    const DNSServiceErrorType error_code, [[maybe_unused]] const char* service_name,
    [[maybe_unused]] const char* reg_type, [[maybe_unused]] const char* reply_domain, [[maybe_unused]] void* context
) {
    auto* advertiser = static_cast<bonjour_advertiser*>(context);
    if (error_code == kDNSServiceErr_NameConflict) {
        advertiser->emit(events::name_conflict {reg_type, service_name});
        return;
    }

    if (error_code != kDNSServiceErr_NoError) {
        advertiser->emit(events::advertiser_error {fmt::format("Failed to register service: {}", error_code)});
        return;
    }
}

rav::dnssd::bonjour_advertiser::registered_service*
rav::dnssd::bonjour_advertiser::find_registered_service(const util::id id) {
    for (auto& service : registered_services_) {
        if (service.id == id) {
            return &service;
        }
    }

    return nullptr;
}

void rav::dnssd::bonjour_advertiser::update_txt_record(const util::id id, const txt_record& txt_record) {
    const auto* const service = find_registered_service(id);
    if (service == nullptr) {
        RAV_THROW_EXCEPTION("Service not found");
    }

    auto const record = bonjour_txt_record(txt_record);

    // Second argument's nullptr tells us that we are updating the primary record.
    const auto result =
        DNSServiceUpdateRecord(service->service_ref.service_ref(), nullptr, 0, record.length(), record.bytesPtr(), 0);

    DNSSD_THROW_IF_ERROR(result, "Failed to update TXT record");
}

#endif
