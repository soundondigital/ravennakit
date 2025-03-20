#include "ravennakit/core/assert.hpp"
#include "ravennakit/dnssd/bonjour/bonjour.hpp"

#if RAV_HAS_APPLE_DNSSD

    #include "ravennakit/dnssd/bonjour/bonjour_browser.hpp"
    #include "ravennakit/core/log.hpp"
    #include "ravennakit/dnssd/bonjour/bonjour_scoped_dns_service_ref.hpp"
    #include "ravennakit/dnssd/bonjour/bonjour_txt_record.hpp"

    #include <mutex>

rav::dnssd::BonjourBrowser::service::service(
    const char* fullname, const char* name, const char* type, const char* domain, BonjourBrowser& owner
) :
    owner_(owner) {
    description_.fullname = fullname;
    description_.name = name;
    description_.reg_type = type;
    description_.domain = domain;
}

void rav::dnssd::BonjourBrowser::service::resolve_on_interface(uint32_t index) {
    if (resolvers_.find(index) != resolvers_.end()) {
        RAV_WARNING("Already resolving on interface {}", index);
        return;
    }

    description_.interfaces.insert({index, {}});

    DNSServiceRef resolveServiceRef = owner_.shared_connection_.service_ref();

    const auto result = DNSServiceResolve(
        &resolveServiceRef, kDNSServiceFlagsShareConnection, index, description_.name.c_str(),
        description_.reg_type.c_str(), description_.domain.c_str(), resolve_callback, this
    );

    if (result != kDNSServiceErr_NoError) {
        owner_.emit(BrowseError {fmt::format("Resolve on interface error: {}", dns_service_error_to_string(result))});
        return;
    }

    resolvers_.insert({index, BonjourScopedDnsServiceRef(resolveServiceRef)});
}

void rav::dnssd::BonjourBrowser::service::resolve_callback(
    [[maybe_unused]] DNSServiceRef serviceRef, [[maybe_unused]] DNSServiceFlags flags, uint32_t interface_index,
    DNSServiceErrorType error_code, [[maybe_unused]] const char* fullname, const char* host_target, uint16_t port,
    const uint16_t txt_len, const unsigned char* txt_record, void* context
) {
    auto* browser_service = static_cast<service*>(context);

    if (error_code != kDNSServiceErr_NoError) {
        browser_service->owner_.emit(
            BrowseError {fmt::format("Resolve error: {}", dns_service_error_to_string(error_code))}
        );
        return;
    }

    browser_service->description_.host_target = host_target;
    browser_service->description_.port = ntohs(port);
    browser_service->description_.txt = BonjourTxtRecord::get_txt_record_from_raw_bytes(txt_record, txt_len);

    browser_service->owner_.emit(ServiceResolved {browser_service->description_});

    DNSServiceRef getAddrInfoServiceRef = browser_service->owner_.shared_connection_.service_ref();

    const auto result = DNSServiceGetAddrInfo(
        &getAddrInfoServiceRef, kDNSServiceFlagsShareConnection | kDNSServiceFlagsTimeout, interface_index,
        kDNSServiceProtocol_IPv4 | kDNSServiceProtocol_IPv6, host_target, get_addr_info_callback, browser_service
    );

    if (result != kDNSServiceErr_NoError) {
        browser_service->owner_.emit(
            BrowseError {fmt::format("Get addr info error: {}", dns_service_error_to_string(result))}
        );
        return;
    }

    browser_service->get_addrs_.insert({interface_index, BonjourScopedDnsServiceRef(getAddrInfoServiceRef)});
}

void rav::dnssd::BonjourBrowser::service::get_addr_info_callback(
    [[maybe_unused]] DNSServiceRef sd_ref, [[maybe_unused]] DNSServiceFlags flags, const uint32_t interface_index,
    const DNSServiceErrorType error_code, [[maybe_unused]] const char* hostname, const struct sockaddr* address,
    [[maybe_unused]] uint32_t ttl, void* context
) {
    auto* browser_service = static_cast<service*>(context);

    if (error_code == kDNSServiceErr_Timeout) {
        browser_service->get_addrs_.erase(interface_index);
        return;
    }

    if (error_code != kDNSServiceErr_NoError) {
        browser_service->owner_.emit(
            BrowseError {fmt::format("Get addr info error: {}", dns_service_error_to_string(error_code))}
        );
        return;
    }

    char ip_addr[INET6_ADDRSTRLEN] = {};

    const void* ip_addr_data = nullptr;

    if (address->sa_family == AF_INET) {
        ip_addr_data = &reinterpret_cast<const sockaddr_in*>(address)->sin_addr;
    } else if (address->sa_family == AF_INET6) {
        ip_addr_data = &reinterpret_cast<const sockaddr_in6*>(address)->sin6_addr;
    } else {
        return;  // Don't know how to handle this case
    }

    // Winsock version requires the const cast because Microsoft.
    inet_ntop(address->sa_family, const_cast<void*>(ip_addr_data), ip_addr, INET6_ADDRSTRLEN);

    const auto found_interface = browser_service->description_.interfaces.find(interface_index);
    if (found_interface != browser_service->description_.interfaces.end()) {
        const auto result = found_interface->second.insert(ip_addr);
        browser_service->owner_.emit(AddressAdded {browser_service->description_, *result.first, interface_index});
    } else {
        browser_service->owner_.emit(BrowseError {fmt::format("Interface with id \"{}\" not found", interface_index)});
        return;
    }
}

size_t rav::dnssd::BonjourBrowser::service::remove_interface(uint32_t index) {
    const auto found_interface = description_.interfaces.find(index);
    if (found_interface == description_.interfaces.end()) {
        RAV_ERROR("Interface with id \"{}\" not found", index);
        return description_.interfaces.size();
    }

    if (description_.interfaces.size() > 1) {
        for (auto& addr : found_interface->second) {
            owner_.emit(AddressRemoved {description_, addr, index});
        }
    }

    description_.interfaces.erase(found_interface);
    resolvers_.erase(index);
    get_addrs_.erase(index);

    return description_.interfaces.size();
}

const rav::dnssd::ServiceDescription& rav::dnssd::BonjourBrowser::service::description() const noexcept {
    return description_;
}

rav::dnssd::BonjourBrowser::BonjourBrowser(asio::io_context& io_context) : service_socket_(io_context) {
    const int service_fd = DNSServiceRefSockFD(shared_connection_.service_ref());

    if (service_fd < 0) {
        RAV_THROW_EXCEPTION("Invalid file descriptor");
    }

    service_socket_.assign(asio::ip::tcp::v6(), service_fd);
    async_process_results();
}

void rav::dnssd::BonjourBrowser::async_process_results() {
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
            emit(BrowseError {fmt::format("Process result error: {}", dns_service_error_to_string(result))});
            if (++process_results_failed_attempts_ > 10) {
                RAV_ERROR("Too many failed attempts to process results, stopping");
                emit(BrowseError {"Too many failed attempts to process results, stopping"});
                return;
            }
        } else {
            process_results_failed_attempts_ = 0;
        }

        async_process_results();
    });
}

void rav::dnssd::BonjourBrowser::browse_reply(
    [[maybe_unused]] DNSServiceRef browse_service_ref, DNSServiceFlags flags, uint32_t interface_index,
    const DNSServiceErrorType error_code, const char* name, const char* type, const char* domain, void* context
) {
    auto* browser = static_cast<BonjourBrowser*>(context);

    if (error_code != kDNSServiceErr_NoError) {
        browser->emit(
            BrowseError {fmt::format("Browser repy called with error: {}", dns_service_error_to_string(error_code))}
        );
        return;
    }

    RAV_TRACE(
        "browse_reply name={} type={} domain={} browse_service_ref={} interfaceIndex={}", name, type, domain,
        static_cast<void*>(browse_service_ref), interface_index
    );

    char fullname[kDNSServiceMaxDomainName] = {};
    const auto result = DNSServiceConstructFullName(fullname, name, type, domain);
    if (result != kDNSServiceErr_NoError) {
        browser->emit(
            BrowseError {fmt::format("Failed to construct full name: {}", dns_service_error_to_string(result))}
        );
        return;
    }

    if (flags & kDNSServiceFlagsAdd) {
        // Insert a new service if not already present
        auto s = browser->services_.find(fullname);
        if (s == browser->services_.end()) {
            s = browser->services_.insert({fullname, service(fullname, name, type, domain, *browser)}).first;

            browser->emit(ServiceDiscovered {s->second.description()});
        }

        s->second.resolve_on_interface(interface_index);
    } else {
        auto const foundService = browser->services_.find(fullname);
        if (foundService == browser->services_.end()) {
            browser->emit(BrowseError {fmt::format("Service with fullname \"{}\" not found", fullname)});
        }

        if (foundService->second.remove_interface(interface_index) == 0) {
            // We just removed the last interface
            browser->emit(ServiceRemoved {foundService->second.description()});

            // Remove the BrowseResult (as there are not interfaces left)
            browser->services_.erase(foundService);
        }
    }
}

void rav::dnssd::BonjourBrowser::browse_for(const std::string& service) {
    DNSServiceRef browsing_service_ref = shared_connection_.service_ref();

    if (browsing_service_ref == nullptr) {
        RAV_THROW_EXCEPTION("DNSSD Service not running");
    }

    if (browsers_.find(service) != browsers_.end()) {
        // Already browsing for this service
        RAV_THROW_EXCEPTION("Already browsing for service \"{}\"", service);
    }

    DNSSD_THROW_IF_ERROR(
        DNSServiceBrowse(
            &browsing_service_ref, kDNSServiceFlagsShareConnection, kDNSServiceInterfaceIndexAny, service.c_str(),
            nullptr, browse_reply, this
        ),
        "Browse error"
    );

    browsers_.insert({service, BonjourScopedDnsServiceRef(browsing_service_ref)});
    // From here the serviceRef is under RAII inside the ScopedDnsServiceRef class
}

const rav::dnssd::ServiceDescription* rav::dnssd::BonjourBrowser::find_service(const std::string& service_name
) const {
    for (auto& service : services_) {
        if (service.second.description().name == service_name) {
            return &service.second.description();
        }
    }
    return nullptr;
}

std::vector<rav::dnssd::ServiceDescription> rav::dnssd::BonjourBrowser::get_services() const {
    std::vector<ServiceDescription> result;
    for (auto& service : services_) {
        result.push_back(service.second.description());
    }
    return result;
}

void rav::dnssd::BonjourBrowser::subscribe(Subscriber& s) {
    subscribers_.push_back(s);
    for (auto& [fullname, service] : services_) {
        s->emit(ServiceDiscovered {service.description()});
        s->emit(ServiceResolved {service.description()});
        for (auto& [iface_index, addrs] : service.description().interfaces) {
            for (auto& addr : addrs) {
                s->emit(AddressAdded {service.description(), addr, iface_index});
            }
        }
    }
}

#endif
