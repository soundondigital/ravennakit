/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/datasets/ptp_port_ds.hpp"
#include "ravennakit/ptp/detail/ptp_request_response_delay_sequence.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::ptp::RequestResponseDelaySequence") {
    SECTION("Test two-step sequence") {
        const auto t1 = rav::ptp::Timestamp(10, 0);  // Sync send time
        const auto t2 = rav::ptp::Timestamp(11, 0);  // Sync receive time
        const auto t3 = rav::ptp::Timestamp(12, 0);  // Delay req send time
        const auto t4 = rav::ptp::Timestamp(14, 0);  // Delay resp receive time

        rav::ptp::SyncMessage sync_message;
        sync_message.header.flags.two_step_flag = true;
        sync_message.receive_timestamp = t2;

        rav::ptp::FollowUpMessage follow_up_message;
        follow_up_message.precise_origin_timestamp = t1;

        rav::ptp::DelayRespMessage delay_resp_message;
        delay_resp_message.receive_timestamp = t4;

        rav::ptp::RequestResponseDelaySequence seq(sync_message);
        REQUIRE(seq.get_state() == rav::ptp::RequestResponseDelaySequence::state::awaiting_follow_up);

        seq.update(follow_up_message);
        REQUIRE(seq.get_state() == rav::ptp::RequestResponseDelaySequence::state::ready_to_be_scheduled);

        seq.schedule_delay_req_message_send({});
        REQUIRE(seq.get_state() == rav::ptp::RequestResponseDelaySequence::state::delay_req_send_scheduled);

        seq.set_delay_req_sent_time(t3);
        REQUIRE(seq.get_state() == rav::ptp::RequestResponseDelaySequence::state::awaiting_delay_resp);

        seq.update(delay_resp_message);
        REQUIRE(seq.get_state() == rav::ptp::RequestResponseDelaySequence::state::delay_resp_received);

        auto mean_delay = seq.calculate_mean_path_delay();
        REQUIRE(rav::is_within(mean_delay,1.5, 0.0));
    }
}
