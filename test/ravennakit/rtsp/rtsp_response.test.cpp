/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/rtsp_response.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rtsp_response", "[rtsp_response]") {
    SECTION("reset") {
        rav::rtsp_response request;
        request.status_code = 404;
        request.reason_phrase = "Error";
        request.rtsp_version_major = 1;
        request.rtsp_version_minor = 1;
        request.headers.emplace_back({"CSeq", "1"});
        request.data = "Hello, World!";
        request.reset();
        REQUIRE(request.status_code == 0);
        REQUIRE(request.reason_phrase.empty());
        REQUIRE(request.rtsp_version_major == 0);
        REQUIRE(request.rtsp_version_minor == 0);
        REQUIRE(request.headers.empty());
        REQUIRE(request.data.empty());
    }
}
