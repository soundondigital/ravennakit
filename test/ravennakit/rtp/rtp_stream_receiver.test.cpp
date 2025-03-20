/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/rtp_stream_receiver.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rtp_stream_receiver") {
    asio::io_context io_context;
    rav::rtp::Receiver receiver(io_context, {});
    rav::rtp::StreamReceiver stream_receiver(receiver);
}
