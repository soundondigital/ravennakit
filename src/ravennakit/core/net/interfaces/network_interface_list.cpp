/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/net/interfaces/network_interface_list.hpp"

#include "ravennakit/core/log.hpp"

rav::network_interface_list::network_interface_list() {
    refresh();
}

const rav::network_interface* rav::network_interface_list::find_by_string(const std::string& search_string) const {
    if (search_string.empty()) {
        return nullptr;
    }

    // Match identifier
    for (auto& interface : interfaces_) {
        if (interface.identifier() == search_string) {
            return &interface;
        }
    }

    // Match display name
    for (auto& interface : interfaces_) {
        if (interface.display_name() == search_string) {
            return &interface;
        }
    }

    // Match description
    for (auto& interface : interfaces_) {
        if (interface.description() == search_string) {
            return &interface;
        }
    }

    // Match MAC address
    for (auto& interface : interfaces_) {
        const auto mac_addr = interface.get_mac_address();
        if (mac_addr && mac_addr->to_string() == search_string) {
            return &interface;
        }
    }

    // Match address
    for (auto& interface : interfaces_) {
        for (const auto& address : interface.addresses()) {
            if (address.to_string() == search_string) {
                return &interface;
            }
        }
    }

    return nullptr;
}

const rav::network_interface* rav::network_interface_list::find_by_address(const asio::ip::address& address) const {
    for (auto& interface : interfaces_) {
        for (const auto& addr : interface.addresses()) {
            if (addr == address) {
                return &interface;
            }
        }
    }
    return nullptr;
}

void rav::network_interface_list::refresh() {
    auto result = network_interface::get_all();
    if (!result) {
        RAV_ERROR("Failed to get network interfaces: {}", result.error());
        return;
    }
    interfaces_ = std::move(result.value());
}

const std::vector<rav::network_interface>& rav::network_interface_list::interfaces() const {
    return interfaces_;
}
