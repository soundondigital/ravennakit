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

rav::ptp_instance::ptp_instance(asio::io_context& io_context) :
    io_context_(io_context), default_ds_(true), parent_ds_(default_ds_) {}

tl::expected<void, rav::ptp_error> rav::ptp_instance::add_port(const asio::ip::address& interface_address) {
    if (!ports_.empty()) {
        return tl::unexpected(ptp_error::only_ordinary_clock_supported);
    }

    network_interfaces_.refresh();
    auto* iface = network_interfaces_.find_by_address(interface_address);
    if (!iface) {
        return tl::unexpected(ptp_error::network_interface_not_found);
    }

    if (default_ds_.clock_identity.empty()) {
        // Need to assign the instance clock identity based on the first port added
        const auto mac_address = iface->get_mac_address();
        if (!mac_address) {
            return tl::unexpected(ptp_error::no_mac_address_available);
        }

        const auto identity = ptp_clock_identity::from_mac_address(mac_address.value());
        if (!identity.is_valid()) {
            return tl::unexpected(ptp_error::invalid_clock_identity);
        }

        default_ds_.clock_identity = identity;
    }

    ptp_port_identity port_identity;
    port_identity.clock_identity = default_ds_.clock_identity;
    port_identity.port_number = get_next_available_port_number();

    ports_.emplace_back(std::make_unique<ptp_port>(*this, io_context_, interface_address, port_identity))
        ->assert_valid_state(ptp_default_profile_1);

    return {};
}

const rav::ptp_parent_ds& rav::ptp_instance::get_parent_ds() const {
    return parent_ds_;
}

void rav::ptp_instance::update_data_sets(
    const ptp_state_decision_code state_decision_code, const ptp_announce_message& announce_message
) {
    current_ds_.update(state_decision_code, announce_message);
    parent_ds_.update(state_decision_code, announce_message);
    time_properties_ds_.update(state_decision_code, announce_message);
}

bool rav::ptp_instance::has_port_identity(const ptp_port_identity& port_identity) const {
    for (auto& port : ports_) {
        if (port->get_port_identity() == port_identity) {
            return true;
        }
    }
    return false;
}

void rav::ptp_instance::execute_state_decision_event() {
    // Note: should be called at least every announce message transmission interval

    const auto all_ports_initializing = std::all_of(ports_.begin(), ports_.end(), [](const auto& port) {
        return port->state() == ptp_state::initializing;
    });

    if (all_ports_initializing) {
        RAV_TRACE("Not executing state decision event because all ports are in slave state");
        return;
    }

    std::vector<ptp_announce_message> best_announce_messages;
    for (const auto& port : ports_) {
        if (auto ebest = port->execute_state_decision_event()) {
            best_announce_messages.push_back(ebest.value());
        }
    }

    // TODO: Compute Ebest
    // TODO: Apply state decision algorithm
    // TODO: Update data sets for all ports
    // TODO: Instantiate the recommended state event in the state machine and make required changes in all PTP ports
}

uint16_t rav::ptp_instance::get_next_available_port_number() const {
    for (uint16_t i = ptp_port_identity::k_port_number_min; i <= ptp_port_identity::k_port_number_max; ++i) {
        if (std::none_of(ports_.begin(), ports_.end(), [i](const auto& port) {
                return port->get_port_identity().port_number == i;
            })) {
            return i;
        }
    }
    RAV_ASSERT_FALSE("Failed to find the next available port number");
    return 0;
}
