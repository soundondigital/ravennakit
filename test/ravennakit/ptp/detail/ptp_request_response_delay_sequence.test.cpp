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
        constexpr int64_t t1 = 1;   // Sync send time
        constexpr int64_t t2 = 10;  // Sync receive time
        constexpr int64_t t3 = 11;  // Delay req send time
        constexpr int64_t t4 = 12;  // Delay resp receive time

        constexpr auto expected_mean_delay = ((t2 - t3) + (t4 - t1)) / 2;
        constexpr auto expected_offset = (t2 - t1) - expected_mean_delay;

        INFO("Expected mean delay: " << expected_mean_delay);
        INFO("Expected offset: " << expected_offset);

        auto seq = get_finished_sequence(
            rav::ptp_timestamp(t1, 0), rav::ptp_timestamp(t2, 0), rav::ptp_timestamp(t3, 0), rav::ptp_timestamp(t4, 0)
        );

        auto measurement = seq.calculate_offset_from_master();

        REQUIRE(measurement.mean_delay.total_nanos() == expected_mean_delay * 1'000'000'000);
        REQUIRE(measurement.offset_from_master.total_nanos() == expected_offset * 1'000'000'000);
    }
}
