/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/dnssd/mock/dnssd_mock_browser.hpp"

#include "ravennakit/core/exception.hpp"

rav::dnssd::MockBrowser::MockBrowser(boost::asio::io_context& io_context) : io_context_(io_context) {}

void rav::dnssd::MockBrowser::mock_discovering_service(
    const std::string& fullname, const std::string& name, const std::string& reg_type, const std::string& domain
) {
    boost::asio::dispatch(io_context_, [=] {
        if (browsers_.find(reg_type) == browsers_.end()) {
            RAV_THROW_EXCEPTION("Not browsing for reg_type: {}", reg_type);
        }
        dnssd::ServiceDescription service;
        service.fullname = fullname;
        service.name = name;
        service.reg_type = reg_type;
        service.domain = domain;
        const auto [it, inserted] = services_.emplace(fullname, service);
        emit(ServiceDiscovered {it->second});
    });
}

void rav::dnssd::MockBrowser::mock_resolved_service(
    const std::string& fullname, const std::string& host_target, const uint16_t port, const TxtRecord& txt_record
) {
    boost::asio::dispatch(io_context_, [=] {
        const auto it = services_.find(fullname);
        if (it == services_.end()) {
            RAV_THROW_EXCEPTION("Service not discovered: {}", fullname);
        }
        it->second.host_target = host_target;
        it->second.port = port;
        it->second.txt = txt_record;
        emit(ServiceResolved {it->second});
    });
}

void rav::dnssd::MockBrowser::mock_adding_address(
    const std::string& fullname, const std::string& address, const uint32_t interface_index
) {
    boost::asio::dispatch(io_context_, [=] {
        const auto it = services_.find(fullname);
        if (it == services_.end()) {
            RAV_THROW_EXCEPTION("Service not discovered: {}", fullname);
        }
        it->second.interfaces[interface_index].insert(address);
        emit(AddressAdded {it->second, address, interface_index});
    });
}

void rav::dnssd::MockBrowser::mock_removing_address(
    const std::string& fullname, const std::string& address, uint32_t interface_index
) {
    boost::asio::dispatch(io_context_, [=] {
        const auto it = services_.find(fullname);
        if (it == services_.end()) {
            RAV_THROW_EXCEPTION("Service not discovered: {}", fullname);
        }
        const auto iface = it->second.interfaces.find(interface_index);
        if (iface == it->second.interfaces.end()) {
            RAV_THROW_EXCEPTION("Interface not found: {}", std::to_string(interface_index));
        }
        const auto addr = iface->second.find(address);
        if (addr == iface->second.end()) {
            RAV_THROW_EXCEPTION("Address not found: {}", address);
        }
        iface->second.erase(addr);
        if (iface->second.empty()) {
            it->second.interfaces.erase(iface);
        }
        emit(AddressRemoved {it->second, address, interface_index});
    });
}

void rav::dnssd::MockBrowser::mock_removing_service(const std::string& fullname) {
    boost::asio::dispatch(io_context_, [=] {
        const auto it = services_.find(fullname);
        if (it == services_.end()) {
            RAV_THROW_EXCEPTION("Service not discovered: {}", fullname);
        }
        emit(ServiceRemoved {it->second});
        services_.erase(it);
    });
}

void rav::dnssd::MockBrowser::browse_for(const std::string& service_type) {
    auto [it, inserted] = browsers_.insert(service_type);
    if (!inserted) {
        RAV_THROW_EXCEPTION("Service type already being browsed for: {}", service_type);
    }
}

const rav::dnssd::ServiceDescription* rav::dnssd::MockBrowser::find_service(const std::string& service_name) const {
    for (auto& [_, service] : services_) {
        if (service.name == service_name) {
            return &service;
        }
    }
    return nullptr;
}

std::vector<rav::dnssd::ServiceDescription> rav::dnssd::MockBrowser::get_services() const {
    std::vector<ServiceDescription> result;
    for (auto& [_, service] : services_) {
        result.push_back(service);
    }
    return result;
}
