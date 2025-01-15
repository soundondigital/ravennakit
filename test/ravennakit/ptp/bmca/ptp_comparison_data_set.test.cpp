/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/bmca/ptp_comparison_data_set.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("ptp_comparison_data_set") {
    rav::ptp_comparison_data_set a;
    a.grandmaster_priority1 = 128;
    a.grandmaster_identity.data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    a.grandmaster_clock_quality.clock_class = 0x12;
    a.grandmaster_clock_quality.clock_accuracy = rav::ptp_clock_accuracy::lt_10_ns;
    a.grandmaster_clock_quality.offset_scaled_log_variance = 0x1234;
    a.grandmaster_priority2 = 128;
    a.steps_removed = 10;
    a.identity_of_senders.data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    a.identity_of_receiver.clock_identity.data = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    a.identity_of_receiver.port_number = 2;
    rav::ptp_comparison_data_set b = a;  // Start with equal sets

    SECTION("Grandmaster identity is equal") {
        SECTION("Steps removed of a is better") {
            a.steps_removed -= 2;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::better);
        }
        SECTION("Steps removed of a is worse") {
            a.steps_removed += 2;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::worse);
        }

        SECTION("Steps removed of a is better") {
            a.steps_removed--;
            SECTION("Receiver < Sender") {
                b.identity_of_receiver.clock_identity.data[0] = 0x00;
                REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::better);
            }
            SECTION("Receiver > Sender") {
                b.identity_of_receiver.clock_identity.data[0] = 0x02;
                REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::better_by_topology);
            }
        }

        SECTION("Steps removed of a is worse") {
            a.steps_removed++;
            SECTION("Receiver < Sender") {
                a.identity_of_receiver.clock_identity.data[0] = 0x00;
                REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::worse);
            }
            SECTION("Receiver > Sender") {
                a.identity_of_receiver.clock_identity.data[0] = 0x02;
                REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::worse_by_topology);
            }
        }

        SECTION("A has better sender identity") {
            a.identity_of_senders.data[0] = 0x02;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::worse_by_topology);
        }

        SECTION("A has worse sender identity") {
            a.identity_of_senders.data[0] = 0x00;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::better_by_topology);
        }

        SECTION("A has higher port number") {
            a.identity_of_receiver.port_number++;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::worse_by_topology);
        }

        SECTION("A has lower port number") {
            a.identity_of_receiver.port_number--;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::better_by_topology);
        }
    }

    SECTION("Grandmaster identity is not equal") {
        b.grandmaster_identity.data[0] = 0x00;

        SECTION("GM Priority1 of a is better") {
            a.grandmaster_priority1--;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::better);
        }

        SECTION("GM Priority1 of a is worse") {
            a.grandmaster_priority1++;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::worse);
        }

        SECTION("GM Clock class of a is better") {
            a.grandmaster_clock_quality.clock_class--;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::better);
        }

        SECTION("GM Clock class of a is worse") {
            a.grandmaster_clock_quality.clock_class++;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::worse);
        }

        SECTION("GM Clock accuracy of a is better") {
            a.grandmaster_clock_quality.clock_accuracy = rav::ptp_clock_accuracy::lt_2_5_ns;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::better);
        }

        SECTION("GM Clock accuracy of a is worse") {
            a.grandmaster_clock_quality.clock_accuracy = rav::ptp_clock_accuracy::lt_25_ns;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::worse);
        }

        SECTION("GM Offset scaled log variance of a is better") {
            a.grandmaster_clock_quality.offset_scaled_log_variance--;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::better);
        }

        SECTION("GM Offset scaled log variance of a is worse") {
            a.grandmaster_clock_quality.offset_scaled_log_variance++;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::worse);
        }

        SECTION("GM Priority2 of a is better") {
            a.grandmaster_priority2--;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::better);
        }

        SECTION("GM Priority2 of a is worse") {
            a.grandmaster_priority2++;
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::worse);
        }

        SECTION("Tie breaker") {
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::better);
        }

        b.grandmaster_identity.data[0] = 0x02;

        SECTION("Tie breaker") {
            REQUIRE(a.compare(b) == rav::ptp_comparison_data_set::result::worse);
        }
    }

    SECTION("Order of ordering") {
        using ordering = rav::ptp_comparison_data_set::result;
        REQUIRE(ordering::worse_by_topology > ordering::worse);
        REQUIRE(ordering::error1 > ordering::worse_by_topology);
        REQUIRE(ordering::error2 > ordering::error1);
        REQUIRE(ordering::better_by_topology > ordering::error2);
        REQUIRE(ordering::better > ordering::better_by_topology);
    }
}
