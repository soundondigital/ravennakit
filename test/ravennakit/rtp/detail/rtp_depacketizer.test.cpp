/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/detail/rtp_depacketizer.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rtp_depacketizer") {
    rav::rtp_receive_buffer buffer;
    rav::rtp_depacketizer depacketizer(buffer);
}
