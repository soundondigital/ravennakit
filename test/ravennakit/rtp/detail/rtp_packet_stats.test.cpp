/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/rtp_packet.hpp"
#include "ravennakit/rtp/detail/rtp_packet_stats.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rtp_packet_stats") {
    SECTION("Basic sequence") {
        rav::rtp_packet_stats stats;
        stats.update(10);
        stats.update(11);
        stats.update(12);
        stats.update(13);
        stats.update(14);
        const auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 0);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("Drop one packet") {
        rav::rtp_packet_stats stats;
        stats.update(10);
        stats.update(12);
        auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 1);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 0);

        // If the older packet eventually arrives it was not dropped, but out of order
        stats.update(11);

        totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 0);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 1);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("Drop two packets") {
        rav::rtp_packet_stats stats;
        stats.update(10);
        stats.update(13);
        auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 2);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 0);

        stats.update(14);
        stats.update(15);
        stats.update(16);
        stats.update(17);
        stats.update(12);

        totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 1);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 1);
        REQUIRE(totals.too_late == 0);

        stats.update(11);
        totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 0);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 2);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("A packet older than the first packet is dropped") {
        rav::rtp_packet_stats stats;
        stats.update(10);
        // Ideally this packet should not be marked duplicate, but I can;t think of a simple, clean and easy way to
        // implement this so for now we just mark it as duplicate. The chance of this happening is very low anyway.
        stats.update(9);
        const auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 0);
        REQUIRE(totals.duplicates == 1);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("Too old") {
        rav::rtp_packet_stats stats;
        stats.update(10);
        stats.update(11);
        stats.update(12);
        stats.update(13);
        stats.update(14);
        stats.update(15);
        stats.update(10);
        const auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 0);
        REQUIRE(totals.duplicates == 1);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("Drop, out of order, duplicates and too old") {
        rav::rtp_packet_stats stats;
        stats.update(10);
        stats.update(15);
        stats.update(10);  // Duplicate
        stats.update(13);  // Out of order
        stats.update(13);  // Our of order and duplicate

        // Move existing values out of the window
        stats.update(16);
        stats.update(17);
        stats.update(18);
        stats.update(19);
        stats.update(20);

        const auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 3);  // Seq 11, 12 and 14 are dropped
        REQUIRE(totals.duplicates == 2);
        REQUIRE(totals.out_of_order == 1);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("Test wrap around") {
        rav::rtp_packet_stats stats;
        stats.update(0xffff - 2);
        stats.update(0xffff - 1);
        stats.update(0xffff);
        stats.update(0x0);
        auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 0);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("Test wrap around with drop") {
        rav::rtp_packet_stats stats;
        stats.update(0xffff - 2);
        stats.update(0xffff - 1);
        stats.update(0xffff);
        stats.update(0x1);
        stats.update(0x2);
        stats.update(0x3);
        stats.update(0x4);
        stats.update(0x5);
        auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 1);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("Test wrap around with drop, out of order, duplicates and too old") {
        rav::rtp_packet_stats stats;
        stats.update(0xffff - 2);
        stats.update(0x1);         // Jumping 4 packets
        stats.update(0x1);         // Duplicate
        stats.update(0xffff - 1);  // Out of order
        stats.update(0xffff);      // Out of order
        stats.update(0x2);
        stats.update(0x3);
        stats.update(0x4);
        stats.update(0x5);
        auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 1);  // 0x0
        REQUIRE(totals.duplicates == 1);
        REQUIRE(totals.out_of_order == 2);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("Mark too late") {
        rav::rtp_packet_stats stats;
        stats.update(1);
        stats.mark_packet_too_late(0);
        stats.mark_packet_too_late(1);
        stats.mark_packet_too_late(2);  // Too new
        auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 0);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 2);

        stats.update(2);
        stats.update(3);

        totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 0);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 2);
    }

    SECTION("Count 1 for every case") {
        rav::rtp_packet_stats stats;
        stats.update(1);
        stats.update(4);
        stats.update(3);  // Out of order
        stats.update(5);
        stats.update(5);  // Duplicate
        stats.update(1);  // Duplicate and out of order
        stats.mark_packet_too_late(3);

        // Slide the window so all values from the current window are collected in the totals
        stats.update(6);
        stats.update(7);
        stats.update(8);
        stats.update(9);
        const auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 1);
        REQUIRE(totals.duplicates == 2);
        REQUIRE(totals.out_of_order == 1);
        REQUIRE(totals.too_late == 1);
    }

    SECTION("Handling duplicates across the window") {
        rav::rtp_packet_stats stats;
        stats.update(100);
        stats.update(101);
        stats.update(101);
        stats.update(102);
        stats.update(102);
        stats.update(102);
        auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 0);
        REQUIRE(totals.duplicates == 3);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("Extreme out-of-order packets") {
        rav::rtp_packet_stats stats;
        stats.update(200);
        stats.update(205);
        stats.update(202);
        stats.update(204);
        stats.update(203);
        auto totals = stats.get_total_counts();
        REQUIRE(totals.out_of_order == 3);
    }

    SECTION("Reset behavior") {
        rav::rtp_packet_stats stats;
        stats.update(10);
        stats.update(12);
        stats.update(14);
        stats.mark_packet_too_late(12);
        stats.reset();
        auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 0);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("Reset with new window size") {
        rav::rtp_packet_stats stats;
        stats.update(1);
        stats.update(2);
        stats.update(3);
        stats.update(4);
    }

    SECTION("Marking packets too late before arrival") {
        rav::rtp_packet_stats stats;
        stats.mark_packet_too_late(50);
        stats.update(50);
        stats.mark_packet_too_late(50);
        auto totals = stats.get_total_counts();
        REQUIRE(totals.too_late == 1);
    }

    SECTION("Continuous window updates with wraparound") {
        rav::rtp_packet_stats stats;
        for (uint16_t i = 0; i < 10; i++) {
            stats.update(0xfff0 + i * 2);
        }
        auto counts = stats.get_total_counts();
        REQUIRE(counts.dropped > 0);
    }

    SECTION("Handling maximum window size") {
        rav::rtp_packet_stats stats;
        stats.update(0);
        stats.update(32767);
        auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 32766);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 0);

        stats.update(65535);
        totals = stats.get_total_counts();
        REQUIRE(totals.dropped == 65533);
        REQUIRE(totals.duplicates == 0);
        REQUIRE(totals.out_of_order == 0);
        REQUIRE(totals.too_late == 0);
    }

    SECTION("Test specific bug where the amount duplicates drops would suddenly jump to weird numbers") {
        rav::rtp_packet_stats stats;

        for (uint16_t i = 0; i < 0xffff; i++) {
            stats.update(i);
            INFO("i: " << i);
            REQUIRE(stats.get_total_counts() == rav::rtp_packet_stats::counters {});
        }

        stats.reset();

        for (uint16_t i = 0; i < 0xffff; i++) {
            stats.update(i);
            INFO("i: " << i);
            REQUIRE(stats.get_total_counts() == rav::rtp_packet_stats::counters {});
        }
    }

    SECTION("Run couple of sequences, count drops") {
        rav::rtp_packet_stats stats;

        size_t dropped = 0;
        for (size_t i = 0; i < 3 * 0x10000; i++) {
            const auto seq = static_cast<uint16_t>(i);
            if (seq == 0x1) {
                dropped++;
                continue;  // Drop packet
            }
            stats.update(seq);
        }

        const auto totals = stats.get_total_counts();
        REQUIRE(totals.dropped == dropped);
    }

    SECTION("Add counters") {
        rav::rtp_packet_stats::counters a {1, 2, 3, 4};
        rav::rtp_packet_stats::counters b {1, 2, 3, 4};
        auto c = a + b;
        REQUIRE(c.out_of_order == 2);
        REQUIRE(c.duplicates == 4);
        REQUIRE(c.dropped == 6);
        REQUIRE(c.too_late == 8);
    }
}
