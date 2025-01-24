/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/aes67/aes67_packet_time.hpp"
#include "ravennakit/core/util.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("aes67_packet_time") {
    constexpr float eps = 0.005f;
    SECTION("125 microseconds") {
        const auto pt = rav::aes67_packet_time::us_125();
        REQUIRE_THAT(pt.signaled_ptime(44'100), Catch::Matchers::WithinRel(0.136f, eps));
        REQUIRE_THAT(pt.signaled_ptime(48'000), Catch::Matchers::WithinRel(0.125f, eps));
        REQUIRE_THAT(pt.signaled_ptime(88'200), Catch::Matchers::WithinRel(0.136f, eps));
        REQUIRE_THAT(pt.signaled_ptime(96'000), Catch::Matchers::WithinRel(0.125f, eps));
        REQUIRE_THAT(pt.signaled_ptime(192'000), Catch::Matchers::WithinRel(0.125f, eps));
        REQUIRE_THAT(pt.signaled_ptime(384'000), Catch::Matchers::WithinRel(0.125f, eps));

        REQUIRE(pt.framecount(44'100) == 6);
        REQUIRE(pt.framecount(48'000) == 6);
        REQUIRE(pt.framecount(88'200) == 12);
        REQUIRE(pt.framecount(96'000) == 12);
        REQUIRE(pt.framecount(192'000) == 24);
        REQUIRE(pt.framecount(384'000) == 48);
    }

    SECTION("250 microseconds") {
        const auto pt = rav::aes67_packet_time::us_250();
        REQUIRE_THAT(pt.signaled_ptime(44'100), Catch::Matchers::WithinRel(0.272f, eps));
        REQUIRE_THAT(pt.signaled_ptime(48'000), Catch::Matchers::WithinRel(0.250f, eps));
        REQUIRE_THAT(pt.signaled_ptime(88'200), Catch::Matchers::WithinRel(0.272f, eps));
        REQUIRE_THAT(pt.signaled_ptime(96'000), Catch::Matchers::WithinRel(0.250f, eps));
        REQUIRE_THAT(pt.signaled_ptime(192'000), Catch::Matchers::WithinRel(0.250f, eps));
        REQUIRE_THAT(pt.signaled_ptime(384'000), Catch::Matchers::WithinRel(0.250f, eps));

        REQUIRE(pt.framecount(44'100) == 12);
        REQUIRE(pt.framecount(48'000) == 12);
        REQUIRE(pt.framecount(88'200) == 24);
        REQUIRE(pt.framecount(96'000) == 24);
        REQUIRE(pt.framecount(192'000) == 48);
        REQUIRE(pt.framecount(384'000) == 96);
    }

    SECTION("333 microseconds") {
        const auto pt = rav::aes67_packet_time::us_333();
        REQUIRE_THAT(pt.signaled_ptime(44'100), Catch::Matchers::WithinRel(0.363f, eps));
        REQUIRE_THAT(pt.signaled_ptime(48'000), Catch::Matchers::WithinRel(0.333f, eps));
        REQUIRE_THAT(pt.signaled_ptime(88'200), Catch::Matchers::WithinRel(0.363f, eps));
        REQUIRE_THAT(pt.signaled_ptime(96'000), Catch::Matchers::WithinRel(0.333f, eps));
        REQUIRE_THAT(pt.signaled_ptime(192'000), Catch::Matchers::WithinRel(0.333f, eps));
        REQUIRE_THAT(pt.signaled_ptime(384'000), Catch::Matchers::WithinRel(0.333f, eps));

        REQUIRE(pt.framecount(44'100) == 16);
        REQUIRE(pt.framecount(48'000) == 16);
        REQUIRE(pt.framecount(88'200) == 32);
        REQUIRE(pt.framecount(96'000) == 32);
        REQUIRE(pt.framecount(192'000) == 64);
        REQUIRE(pt.framecount(384'000) == 128);
    }

    SECTION("1 millisecond") {
        const auto pt = rav::aes67_packet_time::ms_1();
        REQUIRE_THAT(pt.signaled_ptime(44'100), Catch::Matchers::WithinRel(1.088435411f, eps));
        REQUIRE_THAT(pt.signaled_ptime(48'000), Catch::Matchers::WithinRel(1.0f, eps));
        REQUIRE_THAT(pt.signaled_ptime(88'200), Catch::Matchers::WithinRel(1.088435411f, eps));
        REQUIRE_THAT(pt.signaled_ptime(96'000), Catch::Matchers::WithinRel(1.0f, eps));
        REQUIRE_THAT(pt.signaled_ptime(192'000), Catch::Matchers::WithinRel(1.0f, eps));
        REQUIRE_THAT(pt.signaled_ptime(384'000), Catch::Matchers::WithinRel(1.0f, eps));

        REQUIRE(pt.framecount(44'100) == 48);
        REQUIRE(pt.framecount(48'000) == 48);
        REQUIRE(pt.framecount(88'200) == 96);
        REQUIRE(pt.framecount(96'000) == 96);
        REQUIRE(pt.framecount(192'000) == 192);
        REQUIRE(pt.framecount(384'000) == 384);
    }

    SECTION("4 milliseconds") {
        const auto pt = rav::aes67_packet_time::ms_4();
        REQUIRE_THAT(pt.signaled_ptime(44'100), Catch::Matchers::WithinRel(4.354f, eps));
        REQUIRE_THAT(pt.signaled_ptime(48'000), Catch::Matchers::WithinRel(4.f, eps));
        REQUIRE_THAT(pt.signaled_ptime(88'200), Catch::Matchers::WithinRel(4.354f, eps));
        REQUIRE_THAT(pt.signaled_ptime(96'000), Catch::Matchers::WithinRel(4.f, eps));
        REQUIRE_THAT(pt.signaled_ptime(192'000), Catch::Matchers::WithinRel(4.f, eps));
        REQUIRE_THAT(pt.signaled_ptime(384'000), Catch::Matchers::WithinRel(4.f, eps));

        REQUIRE(pt.framecount(44'100) == 192);
        REQUIRE(pt.framecount(48'000) == 192);
        REQUIRE(pt.framecount(88'200) == 384);
        REQUIRE(pt.framecount(96'000) == 384);
        REQUIRE(pt.framecount(192'000) == 768);
        REQUIRE(pt.framecount(384'000) == 1536);
    }
}
