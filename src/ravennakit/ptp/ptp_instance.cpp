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
#include "ravennakit/ptp/ptp_constants.hpp"

rav::ptp::Instance::Instance(asio::io_context& io_context) :
    io_context_(io_context), state_decision_timer_(io_context), default_ds_(true), parent_ds_(default_ds_) {}

rav::ptp::Instance::~Instance() {
    state_decision_timer_.cancel();
}

tl::expected<void, rav::ptp::Error> rav::ptp::Instance::add_port(const asio::ip::address& interface_address) {
    if (!ports_.empty()) {
        return tl::unexpected(Error::only_ordinary_clock_supported);
    }

    network_interfaces_.refresh();
    auto* iface = network_interfaces_.find_by_address(interface_address);
    if (!iface) {
        return tl::unexpected(Error::network_interface_not_found);
    }

    if (default_ds_.clock_identity.empty()) {
        // Need to assign the instance clock identity based on the first port added
        const auto mac_address = iface->get_mac_address();
        if (!mac_address) {
            return tl::unexpected(Error::no_mac_address_available);
        }

        const auto identity = ClockIdentity::from_mac_address(mac_address.value());
        if (!identity.is_valid()) {
            return tl::unexpected(Error::invalid_clock_identity);
        }

        default_ds_.clock_identity = identity;
    }

    PortIdentity port_identity;
    port_identity.clock_identity = default_ds_.clock_identity;
    port_identity.port_number = get_next_available_port_number();

    ports_.emplace_back(std::make_unique<Port>(*this, io_context_, interface_address, port_identity))
        ->assert_valid_state(DefaultProfile1);

    default_ds_.number_ports = static_cast<uint16_t>(ports_.size());

    if (ports_.size() == 1) {
        // Start the state decision timer
        schedule_state_decision_timer();
    }

    return {};
}

const rav::ptp::DefaultDs& rav::ptp::Instance::default_ds() const {
    return default_ds_;
}

const rav::ptp::ParentDs& rav::ptp::Instance::get_parent_ds() const {
    return parent_ds_;
}

bool rav::ptp::Instance::set_recommended_state(
    const StateDecisionCode state_decision_code, const std::optional<AnnounceMessage>& announce_message
) {
    if (state_decision_code == StateDecisionCode::m1 || state_decision_code == StateDecisionCode::m2) {
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
        time_properties_ds_.time_source = TimeSource::internal_oscillator;
        return true;
    }

    if (state_decision_code == StateDecisionCode::s1) {
        if (!announce_message) {
            RAV_ERROR("State decision code is S1 needs announcement message");
            return false;
        }

        const auto parent_changed = parent_ds_.parent_port_identity != announce_message->header.source_port_identity;
        const auto gm_changed = parent_ds_.grandmaster_identity != announce_message->grandmaster_identity;

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

        if (parent_changed || gm_changed) {
            RAV_INFO("{}", parent_ds_.to_string());
            on_parent_changed({parent_ds_});
        }

        return true;
    }

    return true;
}

void rav::ptp::Instance::execute_state_decision_event() {
    // Note: should be called at least every announce message transmission interval

    const auto all_ports_initializing = std::all_of(ports_.begin(), ports_.end(), [](const auto& port) {
        return port->state() == State::initializing;
    });

    // IEEE1588-2019: 9.2.6.9
    if (all_ports_initializing) {
        RAV_TRACE("Not executing state decision event because all ports are in initializing state");
        return;
    }

    const auto ebest = Port::determine_ebest(ports_);

    for (const auto& port : ports_) {
        RAV_ASSERT(port, "Found a nullptr in the port list");
        port->apply_state_decision_algorithm(default_ds_, ebest);
    }

    // TODO: Update data sets for all ports (currently only one port is supported)
}

bool rav::ptp::Instance::should_process_ptp_messages(const MessageHeader& header) const {
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

rav::ptp::State rav::ptp::Instance::get_state_for_decision_code(const StateDecisionCode code) const {
    switch (code) {
        case StateDecisionCode::m1:
        case StateDecisionCode::m2:
        case StateDecisionCode::m3:
            if (default_ds_.slave_only) {
                return State::listening;  // IEEE 1588-2019: Figure 31
            }
            return State::master;
        case StateDecisionCode::s1:
            return local_ptp_clock_.is_calibrated() ? State::slave : State::uncalibrated;
        case StateDecisionCode::p1:
        case StateDecisionCode::p2:
            if (default_ds_.slave_only) {
                return State::listening;  // IEEE 1588-2019: Figure 31
            }
            return State::passive;
        default:
            return State::undefined;
    }
}

rav::ptp::Timestamp rav::ptp::Instance::get_local_ptp_time() const {
    return local_ptp_clock_.now();
}

rav::ptp::Timestamp rav::ptp::Instance::get_local_ptp_time(const Timestamp local_timestamp) const {
    return local_ptp_clock_.system_to_ptp_time(local_timestamp);
}

void rav::ptp::Instance::update_local_ptp_clock(const Measurement<double>& measurement) {
    current_ds_.mean_delay = TimeInterval::to_fractional_interval(measurement.mean_delay);
    current_ds_.offset_from_master = TimeInterval::to_fractional_interval(measurement.offset_from_master);
    local_ptp_clock_.update(measurement);
}

uint16_t rav::ptp::Instance::get_next_available_port_number() const {
    for (uint16_t i = PortIdentity::k_port_number_min; i <= PortIdentity::k_port_number_max; ++i) {
        if (std::none_of(ports_.begin(), ports_.end(), [i](const auto& port) {
                return port->get_port_identity().port_number == i;
            })) {
            return i;
        }
    }
    RAV_ASSERT_FALSE("Failed to find the next available port number");
    return 0;
}

void rav::ptp::Instance::schedule_state_decision_timer() {
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
