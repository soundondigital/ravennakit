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

namespace {
rav::ptp_request_response_delay_sequence get_finished_sequence(
    const rav::ptp_timestamp& t1, const rav::ptp_timestamp& t2, const rav::ptp_timestamp& t3,
    const rav::ptp_timestamp& t4
) {
    rav::ptp_sync_message sync_message;
    sync_message.header.flags.two_step_flag = true;

    rav::ptp_follow_up_message follow_up_message;
    follow_up_message.precise_origin_timestamp = t1;

    rav::ptp_delay_resp_message delay_resp_message;
    delay_resp_message.receive_timestamp = t4;

    rav::ptp_request_response_delay_sequence seq(sync_message, t2, rav::ptp_port_ds());
    REQUIRE(seq.get_state() == rav::ptp_request_response_delay_sequence::state::awaiting_follow_up);
    seq.update(follow_up_message, rav::ptp_port_ds());
    REQUIRE(seq.get_state() == rav::ptp_request_response_delay_sequence::state::delay_req_send_scheduled);
    seq.set_delay_req_send_time(t3);
    REQUIRE(seq.get_state() == rav::ptp_request_response_delay_sequence::state::awaiting_delay_resp);
    seq.update(delay_resp_message);
    REQUIRE(seq.get_state() == rav::ptp_request_response_delay_sequence::state::delay_resp_received);

    return seq;
}
}  // namespace

TEST_CASE("ptp_request_response_delay_sequence") {
    SECTION("Test calculation of offset and mean delay") {
        const auto t1 = rav::ptp_timestamp(1, 0);   // Sync send time
        const auto t2 = rav::ptp_timestamp(10, 0);  // Sync receive time
        const auto t3 = rav::ptp_timestamp(11, 0);  // Delay req send time
        const auto t4 = rav::ptp_timestamp(12, 0);  // Delay resp receive time

        const auto expected_mean_delay = (t2 - t3 + (t4 - t1)) / 2;
        const auto expected_offset = t2 - t1 - expected_mean_delay;

        INFO("Expected mean delay: " << expected_mean_delay.total_seconds_double());
        INFO("Expected offset: " << expected_offset.total_seconds_double());

        auto seq = get_finished_sequence(t1, t2, t3, t4);

        auto measurement = seq.calculate_offset_from_master();

        REQUIRE(rav::util::is_within(measurement.mean_delay, 5.0, 0.0));
        REQUIRE(rav::util::is_within(measurement.offset_from_master, expected_offset.total_seconds_double(), 0.0));
    }
}
