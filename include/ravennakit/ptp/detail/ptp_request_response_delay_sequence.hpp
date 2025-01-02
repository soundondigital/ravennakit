/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/ptp/messages/ptp_delay_req_message.hpp"
#include "ravennakit/ptp/messages/ptp_follow_up_message.hpp"
#include "ravennakit/ptp/messages/ptp_sync_message.hpp"
#include "ravennakit/core/random.hpp"

namespace rav {

class ptp_request_response_delay_sequence {
  public:
    enum class state {
        sync_received,
        awaiting_follow_up,
        delay_req_send_scheduled,
        awaiting_delay_resp,
        delay_resp_received,
    };

    ptp_request_response_delay_sequence() = default;

    ptp_request_response_delay_sequence(
        const ptp_sync_message& sync_message, const ptp_timestamp sync_receive_time, const ptp_port_ds& port_ds
    ) :
        sync_message_(sync_message), t1_(sync_message.origin_timestamp), t2_(sync_receive_time) {
        if (sync_message.header.flags.two_step_flag) {
            state_ = state::awaiting_follow_up;
        } else {
            // Schedule delay request message for sending since no follow-up message is expected
            schedule_delay_req_message_send(port_ds);
        }
    }

    [[nodiscard]] bool matches(const ptp_message_header& header) const {
        return sync_message_.header.matches(header);
    }

    void update(const ptp_follow_up_message& follow_up_message, const ptp_port_ds& port_ds) {
        RAV_ASSERT(state_ == state::awaiting_follow_up, "State should be awaiting_follow_up");
        t1_ = follow_up_message.precise_origin_timestamp;
        schedule_delay_req_message_send(port_ds);
    }

    [[nodiscard]] ptp_delay_req_message create_delay_req_message(const ptp_port_ds& port_ds) const {
        ptp_delay_req_message delay_req_message;
        delay_req_message.header.source_port_identity = port_ds.port_identity;
        delay_req_message.header = sync_message_.header;
        delay_req_message.header.message_type = ptp_message_type::delay_req;
        delay_req_message.header.message_length = ptp_delay_req_message::k_message_length;
        delay_req_message.header.correction_field = 0;
        delay_req_message.origin_timestamp = sync_message_.origin_timestamp;
        return delay_req_message;
    }

    [[nodiscard]] std::optional<std::chrono::time_point<std::chrono::steady_clock>> get_delay_req_send_time() const {
        if (state_ != state::delay_req_send_scheduled) {
            return std::nullopt;
        }
        return send_delay_req_at_;
    }

  private:
    state state_ = state::sync_received;
    ptp_sync_message sync_message_;
    std::chrono::time_point<std::chrono::steady_clock> send_delay_req_at_ {};  // Should this be a ptp_timestamp?
    ptp_timestamp t1_ {}, t2_ {}, t3_ {}, t4_ {};

    void schedule_delay_req_message_send(const ptp_port_ds& port_ds) {
        const auto max_interval_ms = std::pow(2, port_ds.log_min_delay_req_interval + 1) * 1000;
        const auto interval = random().get_random_interval_ms(0, static_cast<int>(max_interval_ms));
        send_delay_req_at_ = std::chrono::steady_clock::now() + interval;
        state_ = state::delay_req_send_scheduled;
    }
};

}  // namespace rav
