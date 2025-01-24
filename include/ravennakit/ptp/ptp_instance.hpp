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

#include <asio/ip/address.hpp>
#include <tl/expected.hpp>

namespace rav {

/**
 * Represents a PTP instance as defined in IEEE 1588-2019.
 */
class ptp_instance {
  public:
    struct parent_changed_event {
        const ptp_parent_ds& parent;
    };

    struct port_changed_state_event {
        const ptp_port& port;
    };

    event_emitter<parent_changed_event> on_parent_changed;
    event_emitter<port_changed_state_event> on_port_changed_state;

    /**
     * Constructs a PTP instance.
     * @param io_context The asio io context to use for networking and timers. Should be a single-threaded context,
     * multithreaded contexts are not supported and will lead to race conditions.
     */
    explicit ptp_instance(asio::io_context& io_context);

    ~ptp_instance();

    /**
     * Adds a port to the PTP instance. The port will be used to send and receive PTP messages. The clock identity of
     * the PTP instance will be determined by the first port added, based on its MAC address.
     * @param interface_address The address of the interface to bind the port to. The network interface must have a MAC
     * address and support multicast.
     */
    tl::expected<void, ptp_error> add_port(const asio::ip::address& interface_address);

    /**
     * @return The default data set of the PTP instance.
     */
    [[nodiscard]] const ptp_default_ds& default_ds() const;

    /**
     * @returns The parent ds of the PTP instance.
     */
    [[nodiscard]] const ptp_parent_ds& get_parent_ds() const;

    /**
     * Updates the data sets of the PTP instance based on the state decision code.
     * @param state_decision_code The state decision code.
     * @param announce_message The announcement message to update the data sets with.
     */
    bool set_recommended_state(
        ptp_state_decision_code state_decision_code, const std::optional<ptp_announce_message>& announce_message
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
    [[nodiscard]] bool should_process_ptp_messages(const ptp_message_header& header) const;

    /**
     * Determines what the state of a port should be based on the state decision code and the internal state of this
     * instance.
     * @param code State decision code to determine the state for.
     * @return The state the port should be in.
     */
    [[nodiscard]] ptp_state get_state_for_decision_code(ptp_state_decision_code code) const;

    /**
     * @returns The current PTP time from the local PTP clock in nanoseconds.
     */
    [[nodiscard]] ptp_timestamp get_local_ptp_time() const;

    /**
     * Returns the PTP time for given local timestamp.
     * @param local_timestamp The local timestamp.
     * @return The PTP time.
     */
    [[nodiscard]] ptp_timestamp get_local_ptp_time(ptp_timestamp local_timestamp) const;

    /**
     * Adjusts the PTP clock of the PTP instance based on the mean delay and offset from the master.
     * @param measurement The measurement data.
     */
    void update_local_ptp_clock(const ptp_measurement<double>& measurement);

    /**
     * Force updates the local PTP clock to the given timestamp.
     * @param timestamp The timestamp to set the clock to.
     */
    void force_update_local_ptp_clock(ptp_timestamp timestamp);

  private:
    asio::io_context& io_context_;
    asio::steady_timer state_decision_timer_;
    ptp_default_ds default_ds_;
    ptp_current_ds current_ds_;
    ptp_parent_ds parent_ds_;
    ptp_time_properties_ds time_properties_ds_;
    std::vector<std::unique_ptr<ptp_port>> ports_;
    network_interface_list network_interfaces_;
    ptp_local_ptp_clock local_ptp_clock_;

    [[nodiscard]] uint16_t get_next_available_port_number() const;
    void schedule_state_decision_timer();
};

}  // namespace rav
