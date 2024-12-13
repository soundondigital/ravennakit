/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/ptp_instance.hpp"

#include "ravennakit/core/net/interfaces/network_interface.hpp"
#include "ravennakit/core/net/interfaces/network_interface_list.hpp"

rav::ptp_instance::ptp_instance(asio::io_context& io_context) : io_context_(io_context) {}

tl::expected<void, rav::ptp_error> rav::ptp_instance::add_port(const asio::ip::address& address) {
    if (!ports_.empty()) {
        return tl::unexpected(ptp_error::only_ordinary_clock_supported);
    }

    network_interfaces_.refresh();
    auto* iface = network_interfaces_.find_by_address(address);
    if (!iface) {
        return tl::unexpected(ptp_error::network_interface_not_found);
    }

    if (ports_.empty()) {
        // Need to assign the instance clock identity based on the first port added
        const auto mac_address = iface->get_mac_address();
        if (!mac_address) {
            return tl::unexpected(ptp_error::no_mac_address_available);
        }

        auto identity = ptp_clock_identity::from_mac_address(mac_address.value());

        TODO("Store identity in defaultDS");
    }

    ports_.emplace_back(std::make_unique<ptp_port>(io_context_, address));
    return {};
}
