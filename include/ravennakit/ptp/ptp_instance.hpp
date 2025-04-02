/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include "ptp_error.hpp"
#include "ptp_local_ptp_clock.hpp"
#include "ptp_port.hpp"
#include "datasets/ptp_current_ds.hpp"
#include "datasets/ptp_default_ds.hpp"
#include "datasets/ptp_parent_ds.hpp"
#include "datasets/ptp_time_properties_ds.hpp"
#include "ravennakit/core/events.hpp"
#include "ravennakit/core/subscriber_list.hpp"
#include "ravennakit/core/events/event_emitter.hpp"
#include "ravennakit/core/net/interfaces/network_interface_list.hpp"
#include "ravennakit/core/expected.hpp"

#include <asio/ip/address.hpp>

namespace rav::ptp {

/**
 * Represents a PTP instance as defined in IEEE 1588-2019.
 */
class Instance {
  public:
    struct ParentChangedEvent {
        const ParentDs& parent;
    };

    struct PortChangedStateEvent {
        const Port& port;
    };

    EventEmitter<ParentChangedEvent> on_parent_changed;
    EventEmitter<PortChangedStateEvent> on_port_changed_state;

    /**
     * Constructs a PTP instance.
     * @param io_context The asio io context to use for networking and timers. Should be a single-threaded context,
     * multithreaded contexts are not supported and will lead to race conditions.
     */
    explicit Instance(asio::io_context& io_context);

    ~Instance();

    /**
     * Adds a port to the PTP instance. The port will be used to send and receive PTP messages. The clock identity of
     * the PTP instance will be determined by the first port added, based on its MAC address.
     * @param interface_address The address of the interface to bind the port to. The network interface must have a MAC
     * address and support multicast.
     */
    tl::expected<void, Error> add_port(const asio::ip::address& interface_address);

    /**
     * @return The default data set of the PTP instance.
     */
    [[nodiscard]] const DefaultDs& default_ds() const;

    /**
     * @returns The parent ds of the PTP instance.
     */
    [[nodiscard]] const ParentDs& get_parent_ds() const;

    /**
     * Updates the data sets of the PTP instance based on the state decision code.
     * @param state_decision_code The state decision code.
     * @param announce_message The announcement message to update the data sets with.
     */
    bool set_recommended_state(
        StateDecisionCode state_decision_code, const std::optional<AnnounceMessage>& announce_message
    );

    /**
     * Execute a state decision event on all ports in the PTP instance.
     */
    void execute_state_decision_event();

    /**
     * Determines whether the PTP instance should process the given PTP message.
     * @param header The PTP message header.
     * @return True if the PTP instance should process the message, false otherwise.
     */
    [[nodiscard]] bool should_process_ptp_messages(const MessageHeader& header) const;

    /**
     * Determines what the state of a port should be based on the state decision code and the internal state of this
     * instance.
     * @param code State decision code to determine the state for.
     * @return The state the port should be in.
     */
    [[nodiscard]] State get_state_for_decision_code(StateDecisionCode code) const;

    /**
     * @returns The current PTP time from the local PTP clock in nanoseconds.
     */
    [[nodiscard]] Timestamp get_local_ptp_time() const;

    /**
     * Returns the PTP time for given local timestamp.
     * @param local_timestamp The local timestamp.
     * @return The PTP time.
     */
    [[nodiscard]] Timestamp get_local_ptp_time(Timestamp local_timestamp) const;

    /**
     * Adjusts the PTP clock of the PTP instance based on the mean delay and offset from the master.
     * @param measurement The measurement data.
     */
    void update_local_ptp_clock(const Measurement<double>& measurement);

  private:
    asio::io_context& io_context_;
    asio::steady_timer state_decision_timer_;
    DefaultDs default_ds_;
    CurrentDs current_ds_;
    ParentDs parent_ds_;
    TimePropertiesDs time_properties_ds_;
    std::vector<std::unique_ptr<Port>> ports_;
    network_interface_list network_interfaces_;
    LocalSystemClock local_clock_;
    LocalPtpClock local_ptp_clock_ {local_clock_};

    [[nodiscard]] uint16_t get_next_available_port_number() const;
    void schedule_state_decision_timer();
};

}  // namespace rav::ptp
