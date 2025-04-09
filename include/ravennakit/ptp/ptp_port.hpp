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

#include "bmca/ptp_best_announce_message.hpp"
#include "bmca/ptp_foreign_master_list.hpp"
#include "datasets/ptp_parent_ds.hpp"
#include "datasets/ptp_port_ds.hpp"
#include "detail/ptp_basic_filter.hpp"
#include "detail/ptp_request_response_delay_sequence.hpp"
#include "messages/ptp_announce_message.hpp"
#include "messages/ptp_delay_req_message.hpp"
#include "messages/ptp_delay_resp_message.hpp"
#include "messages/ptp_follow_up_message.hpp"
#include "messages/ptp_pdelay_req_message.hpp"
#include "messages/ptp_pdelay_resp_follow_up_message.hpp"
#include "messages/ptp_pdelay_resp_message.hpp"
#include "messages/ptp_sync_message.hpp"
#include "ravennakit/core/containers/ring_buffer.hpp"
#include "ravennakit/rtp/detail/udp_sender_receiver.hpp"
#include "types/ptp_port_identity.hpp"

#include <map>
#include <optional>
#include <vector>

namespace rav::ptp {

class Instance;

class Port {
  public:
    Port(
        Instance& parent, asio::io_context& io_context, const asio::ip::address& interface_address,
        PortIdentity port_identity
    );

    ~Port();

    /**
     * @return The port identity of this port.
     */
    [[nodiscard]] const PortIdentity& get_port_identity() const;

    /**
     * Checks the internal state of this object according to IEEE1588-2019. Asserts when something is wrong.
     */
    void assert_valid_state(const Profile& profile) const;

    /**
     * Applies the state decision algorithm to this port.
     */
    void apply_state_decision_algorithm(const DefaultDs& default_ds, const std::optional<BestAnnounceMessage>& ebest);

    /**
     * @return The current state of this port.
     */
    [[nodiscard]] State state() const;

    /**
     * Calculates erbest of this port, if necessary. Removes entries from the foreign master list which didn't
     * become the best announce message.
     */
    void calculate_erbest();

    /**
     * Finds the best announce message of the given ports.
     * @param ports The ports to find the best announce message from.
     * @return The best announce message, or nullopt if there is no best announce message.
     */
    static std::optional<BestAnnounceMessage> determine_ebest(const std::vector<std::unique_ptr<Port>>& ports);

    /**
     * @return The port data set of this port.
     */
    [[nodiscard]] const PortDs& port_ds() const;

    /**
     * Increases the age by one.
     */
    void increase_age();

    /**
     * Sets the state of this port.
     * @param callback The callback to call when the state changes.
     */
    void on_state_changed(std::function<void(const Port&)> callback);

    /**
     * Sets the interface address of this port.
     * @param interface_address The address of the interface to use.
     */
    void set_interface(const asio::ip::address_v4& interface_address);

  private:
    Instance& parent_;
    PortDs port_ds_;
    asio::steady_timer announce_receipt_timeout_timer_;
    rtp::UdpSenderReceiver event_socket_;
    rtp::UdpSenderReceiver general_socket_;
    std::vector<Subscription> subscriptions_;
    ForeignMasterList foreign_master_list_;
    std::optional<AnnounceMessage> erbest_;
    SlidingStats mean_delay_stats_ {31};
    double mean_delay_ = 0.0;
    BasicFilter mean_delay_filter_ {0.1};
    int32_t syncs_until_delay_req_ = 10;  // Number of syncs until the next delay_req message.
    ByteBuffer send_buffer_ {128};
    std::function<void(const Port&)> on_state_changed_callback_;

    RingBuffer<SyncMessage> sync_messages_ {8};
    RingBuffer<RequestResponseDelaySequence> request_response_delay_sequences_ {8};

    void handle_recv_event(const rtp::UdpSenderReceiver::recv_event& event);
    void handle_announce_message(const AnnounceMessage& announce_message, BufferView<const uint8_t> tlvs);
    void handle_sync_message(SyncMessage sync_message, BufferView<const uint8_t> tlvs);
    void handle_follow_up_message(const FollowUpMessage& follow_up_message, BufferView<const uint8_t> tlvs);
    void handle_delay_resp_message(const DelayRespMessage& delay_resp_message, BufferView<const uint8_t> tlvs);
    void handle_pdelay_resp_message(const PdelayRespMessage& delay_req_message, BufferView<const uint8_t> tlvs);
    void handle_pdelay_resp_follow_up_message(
        const PdelayRespFollowUpMessage& delay_req_message, BufferView<const uint8_t> tlvs
    );

    /**
     * Calculates the recommended state of this port.
     * @param default_ds The default data set of the PTP instance.
     * @param ebest The best announce message of all ports.
     * @return The recommended state of this port.
     */
    [[nodiscard]] std::optional<StateDecisionCode>
    calculate_recommended_state(const DefaultDs& default_ds, const std::optional<ComparisonDataSet>& ebest) const;

    void schedule_announce_receipt_timeout();
    void trigger_announce_receipt_timeout_expires_event();

    void process_request_response_delay_sequence();
    void send_delay_req_message(RequestResponseDelaySequence& sequence);

    void set_state(State new_state);

    [[nodiscard]] Measurement<double> calculate_offset_from_master(const SyncMessage& sync_message) const;

    [[nodiscard]] Measurement<double>
    calculate_offset_from_master(const SyncMessage& sync_message, const FollowUpMessage& follow_up_message) const;
};

}  // namespace rav::ptp
