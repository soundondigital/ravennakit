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

#include "datasets/ptp_port_ds.hpp"
#include "messages/ptp_announce_message.hpp"
#include "messages/ptp_delay_req_message.hpp"
#include "messages/ptp_follow_up_message.hpp"
#include "messages/ptp_message_header.hpp"
#include "messages/ptp_pdelay_req_message.hpp"
#include "messages/ptp_pdelay_resp_follow_up_message.hpp"
#include "messages/ptp_pdelay_resp_message.hpp"
#include "messages/ptp_sync_message.hpp"
#include "ravennakit/rtp/detail/udp_sender_receiver.hpp"
#include "types/ptp_port_identity.hpp"

#include <optional>
#include <vector>

namespace rav {

class ptp_port {
  public:
    ptp_port(asio::io_context& io_context, const asio::ip::address& interface_address, ptp_port_identity port_identity);

    [[nodiscard]] uint16_t get_port_number() const;

    /**
     * Checks the internal state of this object according to IEEE1588-2019. Asserts when something is wrong.
     */
    void assert_valid_state(const ptp_profile& profile) const;

  private:
    ptp_port_ds port_ds_;
    udp_sender_receiver event_socket_;
    udp_sender_receiver general_socket_;
    std::vector<subscription> subscriptions_;

    struct foreign_master_entry {
        ptp_port_identity foreign_master_port_identity;
        size_t foreign_master_announce_messages;
        std::optional<char> most_recent_announce_message;  // Char is a placeholder, update with ptp_announce_message
    };

    std::vector<foreign_master_entry> foreign_master_list_;  // Min capacity: 5

    void handle_recv_event(const udp_sender_receiver::recv_event& event);
    void handle_announce_message(
        const ptp_message_header& header, const ptp_announce_message& announce_message, buffer_view<const uint8_t> tlvs
    );
    void handle_sync_message(
        const ptp_message_header& header, const ptp_sync_message& sync_message, buffer_view<const uint8_t> tlvs
    );
    void handle_delay_req_message(
        const ptp_message_header& header, const ptp_delay_req_message& delay_req_message,
        buffer_view<const uint8_t> tlvs
    );
    void handle_follow_up_message(
        const ptp_message_header& header, const ptp_follow_up_message& follow_up_message,
        buffer_view<const uint8_t> tlvs
    );
    void handle_delay_resp_message(
        const ptp_message_header& header, const ptp_delay_req_message& delay_resp_message,
        buffer_view<const uint8_t> tlvs
    );
    void handle_pdelay_req_message(
        const ptp_message_header& header, const ptp_pdelay_req_message& delay_req_message,
        buffer_view<const uint8_t> tlvs
    );
    void handle_pdelay_resp_message(
        const ptp_message_header& header, const ptp_pdelay_resp_message& delay_req_message,
        buffer_view<const uint8_t> tlvs
    );
    void handle_pdelay_resp_follow_up_message(
        const ptp_message_header& header, const ptp_pdelay_resp_follow_up_message& delay_req_message,
        buffer_view<const uint8_t> tlvs
    );
};

}  // namespace rav
