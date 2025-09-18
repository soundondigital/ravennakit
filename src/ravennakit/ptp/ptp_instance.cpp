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

const rav::ptp::LocalClock& rav::ptp::Instance::Subscriber::get_local_clock() {
    static_assert(std::is_trivially_copyable_v<LocalClock>);
    if (const auto value = local_clock_buffer_.read(boost::lockfree::uses_optional)) {
        local_clock_ = *value;
    }
    return local_clock_;
}

rav::ptp::Instance::Instance(boost::asio::io_context& io_context) :
    io_context_(io_context), state_decision_timer_(io_context), default_ds_(true), parent_ds_(default_ds_) {}

rav::ptp::Instance::~Instance() {
    state_decision_timer_.cancel();
}

bool rav::ptp::Instance::subscribe(Subscriber* subscriber) {
    if (subscribers_.add(subscriber)) {
        if (!parent_ds_.parent_port_identity.is_valid()) {
            return true;  // No parent yet
        }
        subscriber->ptp_parent_changed(parent_ds_);
        for (auto& port : ports_) {
            subscriber->ptp_port_changed_state(*port);
        }
        if (local_ptp_clock_.is_calibrated()) {
            static_assert(std::is_trivially_copyable_v<LocalClock>);
            subscriber->local_clock_buffer_.write(local_clock_);
        }
        return true;
    }
    return false;
}

bool rav::ptp::Instance::unsubscribe(const Subscriber* subscriber) {
    return subscribers_.remove(subscriber);
}

tl::expected<void, rav::ptp::Error>
rav::ptp::Instance::add_port(const uint16_t port_number, const boost::asio::ip::address_v4& interface_address) {
    if (has_port(port_number)) {
        return tl::unexpected(Error::port_already_exists);
    }

    const auto interfaces = NetworkInterfaceList::get_system_interfaces(false);
    auto* iface = interfaces.find_by_address(interface_address);
    if (!iface) {
        return tl::unexpected(Error::network_interface_not_found);
    }

    if (default_ds_.clock_identity.all_zero()) {
        // Need to assign the instance clock identity based on the first port added
        const auto mac_address = iface->get_mac_address();
        if (!mac_address) {
            return tl::unexpected(Error::no_mac_address_available);
        }

        const auto identity = ClockIdentity::from_mac_address(mac_address.value());
        if (!identity) {
            return tl::unexpected(Error::invalid_clock_identity);
        }

        default_ds_.clock_identity = *identity;
    }

    PortIdentity port_identity;
    port_identity.clock_identity = default_ds_.clock_identity;
    port_identity.port_number = port_number;

    auto new_port = std::make_unique<Port>(*this, io_context_, interface_address, port_identity);
    new_port->on_state_changed([this](const Port& port) {
        for (auto* s : subscribers_) {
            s->ptp_port_changed_state(port);
        }
    });

    const auto& added_port = ports_.emplace_back(std::move(new_port));
    added_port->assert_valid_state(DefaultProfile1);

    default_ds_.number_ports = static_cast<uint16_t>(ports_.size());

    if (ports_.size() == 1) {
        // Start the state decision timer
        schedule_state_decision_timer();
    }

    for (auto* s : subscribers_) {
        s->ptp_port_changed_state(*added_port);
    }

    return {};
}

tl::expected<void, rav::ptp::Error> rav::ptp::Instance::add_or_update_port(
    const uint16_t port_number, const boost::asio::ip::address_v4& interface_address
) {
    for (const auto& port : ports_) {
        if (port->get_port_identity().port_number == port_number) {
            port->set_interface(interface_address);
            return {};
        }
    }

    return add_port(port_number, interface_address);
}

tl::expected<void, rav::ptp::Error> rav::ptp::Instance::update_ports(const std::vector<ip_address_v4>& ports) {
    if (ports.size() >= std::numeric_limits<uint16_t>::max()) {
        return tl::unexpected(Error::too_many_ports);
    }

    // Add or update ports
    for (size_t i = 0; i < ports.size(); ++i) {
        const auto port_number = static_cast<uint16_t>(i + 1);
        if (ports[i].is_unspecified()) {
            if (has_port(port_number)) {
                std::ignore = remove_port(port_number);
            }
            continue;
        }
        if (auto result = add_or_update_port(port_number, ports[i]); !result) {
            return tl::unexpected(result.error());
        }
    }

    std::vector<uint16_t> ports_no_longer_in_use;

    for (const auto& port : ports_) {
        auto port_number = port->port_ds().port_identity.port_number;
        if (port_number > ports.size()) {
            ports_no_longer_in_use.push_back(port_number);
        }
    }

    for (auto num : ports_no_longer_in_use) {
        if (!remove_port(num)) {
            RAV_ERROR("Failed to remove port {}", num);
        }
    }

    return {};
}

bool rav::ptp::Instance::has_port(const uint16_t port_number) const {
    for (const auto& port : ports_) {
        if (port->get_port_identity().port_number == port_number) {
            return true;
        }
    }
    return false;
}

bool rav::ptp::Instance::remove_port(uint16_t port_number) {
    const auto it = std::remove_if(ports_.begin(), ports_.end(), [port_number](const auto& port) {
        return port->get_port_identity().port_number == port_number;
    });

    if (it != ports_.end()) {
        ports_.erase(it, ports_.end());
        default_ds_.number_ports = static_cast<uint16_t>(ports_.size());
        RAV_TRACE("Removed port {}, new total amount of ports: {}", port_number, default_ds_.number_ports);

        for (auto* s : subscribers_) {
            s->ptp_port_removed(port_number);
        }

        return true;
    }

    return false;
}

size_t rav::ptp::Instance::get_port_count() const {
    return ports_.size();
}

bool rav::ptp::Instance::set_port_interface(
    const uint16_t port_number, const boost::asio::ip::address_v4& interface_address
) const {
    for (auto& port : ports_) {
        if (port->get_port_identity().port_number == port_number) {
            port->set_interface(interface_address);
            return true;
        }
    }
    return false;
}

const rav::ptp::DefaultDs& rav::ptp::Instance::get_default_ds() const {
    return default_ds_;
}

const rav::ptp::ParentDs& rav::ptp::Instance::get_parent_ds() const {
    return parent_ds_;
}

const rav::ptp::TimePropertiesDs& rav::ptp::Instance::get_time_properties_ds() const {
    return time_properties_ds_;
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
            for (auto* s : subscribers_) {
                s->ptp_parent_changed(parent_ds_);
            }
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
                return State::listening;  // IEEE 1588-2019: Figure 31 (as opposed to Figure 30)
            }
            return State::master;
        case StateDecisionCode::s1:
            return local_ptp_clock_.is_calibrated() ? State::slave : State::uncalibrated;
        case StateDecisionCode::p1:
        case StateDecisionCode::p2:
            if (default_ds_.slave_only) {
                return State::listening;  // IEEE 1588-2019: Figure 31 (as opposed to Figure 30)
            }
            return State::passive;
        default:
            return State::undefined;
    }
}

rav::ptp::Timestamp rav::ptp::Instance::get_local_ptp_time() const {
    return local_clock_.now();
}

void rav::ptp::Instance::update_local_ptp_clock(const Measurement<double>& measurement) {
    current_ds_.mean_delay = TimeInterval::to_fractional_interval(measurement.mean_delay);
    current_ds_.offset_from_master = TimeInterval::to_fractional_interval(measurement.offset_from_master);
    local_ptp_clock_.update(measurement);
    if (local_ptp_clock_.is_calibrated()) {
        for (auto* s : subscribers_) {
            s->local_clock_buffer_.write(local_clock_);
        }
    }
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
    state_decision_timer_.async_wait([this](const boost::system::error_code& error) {
        if (error == boost::asio::error::operation_aborted) {
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
