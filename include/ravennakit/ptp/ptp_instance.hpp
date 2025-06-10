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
#include "ravennakit/core/events/subscriber_list.hpp"
#include "ravennakit/core/expected.hpp"

#include <boost/lockfree/spsc_value.hpp>

namespace rav::ptp {

/**
 * Represents a PTP instance as defined in IEEE 1588-2019.
 */
class Instance {
  public:
    class Subscriber {
      public:
        virtual ~Subscriber() = default;

        /**
         * Called when the parent of the PTP instance changes.
         * @param parent The new parent data set.
         */
        virtual void ptp_parent_changed(const ParentDs& parent) {
            std::ignore = parent;
        }

        /**
         * Called when the state of a port changes.
         * @param port The port that changed state.
         */
        virtual void ptp_port_changed_state(const Port& port) {
            std::ignore = port;
        }

        /**
         * @returns A local copy of the local clock which received updates from the ptp::Instance.
         * Thread safe and wait-free when called from a single consumer thread.
         */
        const LocalClock& get_local_clock();

      private:
        friend class Instance;
        boost::lockfree::spsc_value<LocalClock> local_clock_buffer_;
        LocalClock local_clock_;
    };

    /**
     * Constructs a PTP instance.
     * @param io_context The asio io context to use for networking and timers. Should be a single-threaded context,
     * multithreaded contexts are not supported and will lead to race conditions.
     */
    explicit Instance(boost::asio::io_context& io_context);

    ~Instance();

    /**
     * Adds a subscriber to the PTP instance. The subscriber will be notified of events related to the PTP instance.
     * @param subscriber The subscriber to add.
     * @return True if the subscriber was added successfully, false if the subscriber was already added.
     */
    [[nodiscard]] bool subscribe(Subscriber* subscriber);

    /**
     * Removes a subscriber from the PTP instance. The subscriber will no longer be notified of events related to the
     * PTP instance.
     * @param subscriber The subscriber to remove.
     * @return True if the subscriber was removed successfully, false if the subscriber was not found.
     */
    [[nodiscard]] bool unsubscribe(const Subscriber* subscriber);

    /**
     * Adds a port to the PTP instance. The port will be used to send and receive PTP messages. The clock identity of
     * the PTP instance will be determined by the first port added, based on its MAC address.
     * @param interface_address The address of the interface to bind the port to. The network interface must have a MAC
     * address and support multicast.
     */
    tl::expected<uint16_t, Error> add_port(const boost::asio::ip::address_v4& interface_address);

    /**
     * Removes a port from the PTP instance.
     * @param port_number The port number to remove. The port number is 1-based, so the first port is 1 and 0 is
     * considered invalid.
     * @return True if the port was removed successfully, false if the port was not found.
     */
    bool remove_port(uint16_t port_number);

    /**
     * @return The amount of ports in the PTP instance.
     */
    [[nodiscard]] size_t get_port_count() const;

    /**
     * Sets the network interface for port with given port number.
     * @param port_number The port number to set the interface for. The port number is 1-based, so the first port is 1
     * and 0 is considered invalid.
     * @param interface_address The address of the interface to bind the port to.
     */
    [[nodiscard]] bool set_port_interface(uint16_t port_number, const boost::asio::ip::address_v4& interface_address) const;

    /**
     * @return The default data set of the PTP instance.
     */
    [[nodiscard]] const DefaultDs& get_default_ds() const;

    /**
     * @returns The parent ds of the PTP instance.
     */
    [[nodiscard]] const ParentDs& get_parent_ds() const;

    /**
     * @return The time properties data set of the PTP instance.
     */
    [[nodiscard]] const TimePropertiesDs& get_time_properties_ds() const;

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
     * Adjusts the PTP clock of the PTP instance based on the mean delay and offset from the master.
     * @param measurement The measurement data.
     */
    void update_local_ptp_clock(const Measurement<double>& measurement);

  private:
    boost::asio::io_context& io_context_;
    boost::asio::steady_timer state_decision_timer_;
    DefaultDs default_ds_;
    CurrentDs current_ds_;
    ParentDs parent_ds_;
    TimePropertiesDs time_properties_ds_;
    std::vector<std::unique_ptr<Port>> ports_;
    LocalClock local_clock_;
    LocalPtpClock local_ptp_clock_ {local_clock_};
    SubscriberList<Subscriber> subscribers_;

    [[nodiscard]] uint16_t get_next_available_port_number() const;
    void schedule_state_decision_timer();
};

}  // namespace rav::ptp
