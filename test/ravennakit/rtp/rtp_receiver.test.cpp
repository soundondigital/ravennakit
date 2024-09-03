/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/rtp_receiver.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rtp_receiver::rtp_receiver()", "[rtp_receiver]") {
    auto loop = uvw::loop::create();
    REQUIRE(loop != nullptr);

    SECTION("Start and stop receiving unicast") {
        rav::rtp_receiver receiver(loop);
        receiver.on<rav::rtp_packet_event>([](const rav::rtp_packet_event&, rav::rtp_receiver&) {});
        REQUIRE(receiver.bind("0.0.0.0").is_ok());
        REQUIRE(receiver.close().is_ok());
    }
}
