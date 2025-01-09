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
    io_context_(io_context), state_decision_timer_(io_context), default_ds_(true), parent_ds_(default_ds_) {}

rav::ptp_instance::~ptp_instance() {
    state_decision_timer_.cancel();
}

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

    default_ds_.number_ports = static_cast<uint16_t>(ports_.size());

    if (ports_.size() == 1) {
        // Start the state decision timer
        schedule_state_decision_timer();
    }

    return {};
}

const rav::ptp_default_ds& rav::ptp_instance::default_ds() const {
    return default_ds_;
}

const rav::ptp_parent_ds& rav::ptp_instance::get_parent_ds() const {
    return parent_ds_;
}

bool rav::ptp_instance::set_recommended_state(
    const ptp_state_decision_code state_decision_code, const std::optional<ptp_announce_message>& announce_message
) {
    if (state_decision_code == ptp_state_decision_code::m1 || state_decision_code == ptp_state_decision_code::m2) {
        current_ds_.steps_removed = 0;
        current_ds_.offset_from_master = {};
        current_ds_.mean_delay = {};
        parent_ds_.parent_port_identity.clock_identity = default_ds_.clock_identity;
        parent_ds_.parent_port_identity.port_number = 0;
        parent_ds_.grandmaster_identity = default_ds_.clock_identity;
        parent_ds_.grandmaster_clock_quality = default_ds_.clock_quality;
        parent_ds_.grandmaster_priority1 = default_ds_.priority1;
        parent_ds_.grandmaster_priority2 = default_ds_.priority2;
        time_properties_ds_.leap59 = false;
        time_properties_ds_.leap61 = false;
        time_properties_ds_.time_traceable = false;
        time_properties_ds_.current_utc_offset = 0;
        time_properties_ds_.current_utc_offset_valid = false;
        time_properties_ds_.frequency_traceable = false;
        time_properties_ds_.ptp_timescale = false;
        time_properties_ds_.time_source = ptp_time_source::internal_oscillator;
        return true;
    }

    if (state_decision_code == ptp_state_decision_code::s1) {
        if (!announce_message) {
            RAV_ERROR("State decision code is S1 needs announcement message");
            return false;
        }

        bool log_new_parent = false;
        if (parent_ds_.parent_port_identity != announce_message->header.source_port_identity) {
            log_new_parent = true;
        }
        if (parent_ds_.grandmaster_identity != announce_message->grandmaster_identity) {
            log_new_parent = true;
        }

        current_ds_.steps_removed = 1 + announce_message->steps_removed;
        parent_ds_.parent_port_identity = announce_message->header.source_port_identity;
        parent_ds_.grandmaster_identity = announce_message->grandmaster_identity;
        parent_ds_.grandmaster_clock_quality = announce_message->grandmaster_clock_quality;
        parent_ds_.grandmaster_priority1 = announce_message->grandmaster_priority1;
        parent_ds_.grandmaster_priority2 = announce_message->grandmaster_priority2;
        time_properties_ds_.current_utc_offset = announce_message->current_utc_offset;
        time_properties_ds_.current_utc_offset_valid = announce_message->header.flags.current_utc_offset_valid;
        time_properties_ds_.leap59 = announce_message->header.flags.leap59;
        time_properties_ds_.leap61 = announce_message->header.flags.leap61;
        time_properties_ds_.time_traceable = announce_message->header.flags.time_traceable;
        time_properties_ds_.frequency_traceable = announce_message->header.flags.frequency_traceable;
        time_properties_ds_.ptp_timescale = announce_message->header.flags.ptp_timescale;
        time_properties_ds_.time_source = announce_message->time_source;

        if (log_new_parent) {
            RAV_INFO("{}", parent_ds_.to_string());
        }

        return true;
    }

    return true;
}

void rav::ptp_instance::execute_state_decision_event() {
    // Note: should be called at least every announce message transmission interval

    const auto all_ports_initializing = std::all_of(ports_.begin(), ports_.end(), [](const auto& port) {
        return port->state() == ptp_state::initializing;
    });

    // IEEE1588-2019: 9.2.6.9
    if (all_ports_initializing) {
        RAV_TRACE("Not executing state decision event because all ports are in initializing state");
        return;
    }

    const auto ebest = ptp_port::determine_ebest(ports_);

    for (const auto& port : ports_) {
        RAV_ASSERT(port, "Found a nullptr in the port list");
        port->apply_state_decision_algorithm(default_ds_, ebest);
    }

    // TODO: Update data sets for all ports
    // TODO: Instantiate the recommended state event in the state machine and make required changes in all PTP ports
}

bool rav::ptp_instance::should_process_ptp_messages(const ptp_message_header& header) const {
    // IEEE1588-2019: 7.1.2.1
    if (header.domain_number != default_ds_.domain_number) {
        RAV_TRACE("Discarding message with different domain number: {}", header.to_string());
        return false;
    }

    // IEEE1588-2019: 7.1.2.1
    if (header.sdo_id.major != default_ds_.sdo_id.major) {
        RAV_TRACE("Discarding message with different SDO ID major: {}", header.to_string());
        return false;
    }

    // Not checking sdo_id.minor, since this must only be done when "isolation option of 16.5" is used.

    // IEEE1588-2019: 9.5.2.1
    // Comparing the clock identity, because each port of this instance should have (has) the same clock identity.
    if (header.source_port_identity.clock_identity == default_ds_.clock_identity) {
        RAV_TRACE("Discarding message from own instance: {}", header.to_string());
        return false;
    }

    // IEEE1588-2019: 9.1
    // Unless the alternate master option is active, messages from alternate masters should be discarded.
    if (header.flags.alternate_master_flag) {
        RAV_TRACE("Discarding message with alternate master flag: {}", header.to_string());
        return false;
    }

    return true;
}

rav::ptp_state rav::ptp_instance::get_state_for_decision_code(const ptp_state_decision_code code) const {
    switch (code) {
        case ptp_state_decision_code::m1:
        case ptp_state_decision_code::m2:
        case ptp_state_decision_code::m3:
            if (default_ds_.slave_only) {
                return ptp_state::listening;  // IEEE 1588-2019: Figure 31
            }
            return ptp_state::master;
        case ptp_state_decision_code::s1:
            return local_ptp_clock_.is_calibrated() ? ptp_state::slave : ptp_state::uncalibrated;
        case ptp_state_decision_code::p1:
        case ptp_state_decision_code::p2:
            if (default_ds_.slave_only) {
                return ptp_state::listening;  // IEEE 1588-2019: Figure 31
            }
            return ptp_state::passive;
        default:
            return ptp_state::undefined;
    }
}

rav::ptp_timestamp rav::ptp_instance::get_local_ptp_time() const {
    return local_ptp_clock_.now();
}

void rav::ptp_instance::adjust_ptp_clock(
    const ptp_time_interval mean_delay, const ptp_time_interval offset_from_master
) {
    current_ds_.mean_delay = mean_delay.to_wire_format();
    current_ds_.offset_from_master = offset_from_master.to_wire_format();

    if (std::abs(offset_from_master.seconds()) >= 1) {
        // RAV_WARNING(
            // "Offset from master is too large: {}.{}", offset_from_master.seconds(), offset_from_master.nanos_raw()
        // );
        local_ptp_clock_.step_clock(offset_from_master);
        return;
    }

    local_ptp_clock_.step_clock(offset_from_master);
    // local_ptp_clock_.adjust(offset_from_master);
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

void rav::ptp_instance::schedule_state_decision_timer() {
    if (ports_.empty()) {
        return;  // Basically stopping the timer, which is fine when there are no ports
    }
    const auto announce_interval_seconds = std::pow(2, ports_.front()->port_ds().log_announce_interval);
    state_decision_timer_.expires_after(std::chrono::seconds(static_cast<int>(announce_interval_seconds)));
    state_decision_timer_.async_wait([this](const std::error_code& error) {
        if (error == asio::error::operation_aborted) {
            return;
        }
        if (error) {
            RAV_ERROR("State decision timer error: {}", error.message());
            return;
        }
        execute_state_decision_event();
        for (const auto& port : ports_) {
            port->increase_age();
        }
        schedule_state_decision_timer();
    });
}
