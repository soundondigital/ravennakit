/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/detail/rtsp_response.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rtsp_response", "[rtsp_response]") {
    SECTION("reset") {
        rav::rtsp::Response request;
        request.status_code = 404;
        request.reason_phrase = "Error";
        request.rtsp_version_major = 1;
        request.rtsp_version_minor = 1;
        request.rtsp_headers.emplace_back({"CSeq", "1"});
        request.data = "Hello, World!";
        request.reset();
        REQUIRE(request.status_code == 0);
        REQUIRE(request.reason_phrase.empty());
        REQUIRE(request.rtsp_version_major == 0);
        REQUIRE(request.rtsp_version_minor == 0);
        REQUIRE(request.rtsp_headers.empty());
        REQUIRE(request.data.empty());
    }
}


TEST_CASE("rtsp_response | encode", "[rtsp_response]") {
    rav::rtsp::Response res;
    res.rtsp_version_major = 1;
    res.rtsp_version_minor = 0;
    res.status_code = 200;
    res.reason_phrase = "OK";
    res.rtsp_headers.push_back({"CSeq", "1"});
    res.rtsp_headers.push_back({"Accept", "application/sdp"});
    res.data = "Hello, World!";

    REQUIRE(
        res.encode() == "RTSP/1.0 200 OK\r\nCSeq: 1\r\nAccept: application/sdp\r\ncontent-length: 13\r\n\r\nHello, World!"
    );

    res.rtsp_headers.push_back({"Content-Length", "555"});

    REQUIRE(
         res.encode() == "RTSP/1.0 200 OK\r\nCSeq: 1\r\nAccept: application/sdp\r\ncontent-length: 13\r\n\r\nHello, World!"
    );
}
