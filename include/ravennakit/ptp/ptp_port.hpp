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
#include "detail/ptp_request_response_delay_sequence.hpp"
#include "messages/ptp_announce_message.hpp"
#include "messages/ptp_delay_req_message.hpp"
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

namespace rav {

class ptp_instance;

class ptp_port {
  public:
    ptp_port(
        ptp_instance& parent, asio::io_context& io_context, const asio::ip::address& interface_address,
        ptp_port_identity port_identity
    );

    ~ptp_port();

    /**
     * @return The port identity of this port.
     */
    [[nodiscard]] const ptp_port_identity& get_port_identity() const;

    /**
     * Checks the internal state of this object according to IEEE1588-2019. Asserts when something is wrong.
     */
    void assert_valid_state(const ptp_profile& profile) const;

    /**
     * Applies the state decision algorithm to this port.
     */
    void apply_state_decision_algorithm(
        const ptp_default_ds& default_ds, const std::optional<ptp_best_announce_message>& ebest
    );

    /**
     * @return The current state of this port.
     */
    [[nodiscard]] ptp_state state() const;

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
    static std::optional<ptp_best_announce_message>
    determine_ebest(const std::vector<std::unique_ptr<ptp_port>>& ports);

    /**
     * @return The port data set of this port.
     */
    [[nodiscard]] const ptp_port_ds& port_ds() const;

    /**
     * Increases the age by one.
     */
    void increase_age();

  private:
    ptp_instance& parent_;
    ptp_port_ds port_ds_;
    asio::steady_timer announce_receipt_timeout_timer_;
    asio::steady_timer delay_req_timer_;
    udp_sender_receiver event_socket_;
    udp_sender_receiver general_socket_;
    std::vector<subscription> subscriptions_;
    ptp_foreign_master_list foreign_master_list_;
    std::optional<ptp_announce_message> erbest_;

    ring_buffer<ptp_request_response_delay_sequence> request_response_delay_sequences_ {32};

    void handle_recv_event(const udp_sender_receiver::recv_event& event);
    void handle_announce_message(const ptp_announce_message& announce_message, buffer_view<const uint8_t> tlvs);
    void handle_sync_message(const ptp_sync_message& sync_message, buffer_view<const uint8_t> tlvs);
    void handle_follow_up_message(const ptp_follow_up_message& follow_up_message, buffer_view<const uint8_t> tlvs);
    void handle_delay_resp_message(const ptp_delay_req_message& delay_resp_message, buffer_view<const uint8_t> tlvs);
    void handle_pdelay_resp_message(const ptp_pdelay_resp_message& delay_req_message, buffer_view<const uint8_t> tlvs);
    void handle_pdelay_resp_follow_up_message(
        const ptp_pdelay_resp_follow_up_message& delay_req_message, buffer_view<const uint8_t> tlvs
    );

    /**
     * Calculates the recommended state of this port.
     * @param default_ds The default data set of the PTP instance.
     * @param ebest The best announce message of all ports.
     * @return The recommended state of this port.
     */
    [[nodiscard]] std::optional<ptp_state_decision_code> calculate_recommended_state(
        const ptp_default_ds& default_ds, const std::optional<ptp_comparison_data_set>& ebest
    ) const;

    void schedule_announce_receipt_timeout();
    void trigger_announce_receipt_timeout_expires_event();

    void process_request_response_delay_sequence();
    void send_delay_req_message(ptp_request_response_delay_sequence& sequence);

    void set_state(ptp_state new_state);
};

}  // namespace rav
