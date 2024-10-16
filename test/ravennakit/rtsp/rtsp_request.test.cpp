/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include <catch2/catch_all.hpp>

#include "ravennakit/rtsp/rtsp_request.hpp"

TEST_CASE("rtsp_request", "[rtsp_request]") {
    SECTION("Get header") {
        rav::rtsp_request request;
        request.headers.push_back(rav::rtsp_headers::header {"Content-Length", "123"});
        request.headers.push_back({"Content-Type", "application/sdp"});

        if (const auto* header = request.headers.find_header("Content-Length"); header) {
            REQUIRE(header->value == "123");
        } else {
            FAIL("Content-Length header not found");
        }

        if (const auto* header = request.headers.find_header("Content-Type"); header) {
            REQUIRE(header->value == "application/sdp");
        } else {
            FAIL("Content-Type header not found");
        }

        REQUIRE(request.headers.find_header("Content-Size") == nullptr);
    }

    SECTION("Get content length") {
        rav::rtsp_request request;
        request.headers.push_back({"Content-Length", "123"});

        if (auto content_length = request.headers.get_content_length(); content_length) {
            REQUIRE(*content_length == 123);
        } else {
            FAIL("Content-Length header not found");
        }
    }

    SECTION("Get content length while there is no Content-Length header") {
        rav::rtsp_request request;
        REQUIRE(request.headers.get_content_length() == std::nullopt);
    }

    SECTION("reset") {
        rav::rtsp_request request;
        request.method = "GET";
        request.uri = "/index.html";
        request.rtsp_version_major = 1;
        request.rtsp_version_minor = 1;
        request.headers.emplace_back({"CSeq", "1"});
        request.data = "Hello, World!";
        request.reset();
        REQUIRE(request.method.empty());
        REQUIRE(request.uri.empty());
        REQUIRE(request.rtsp_version_major == 0);
        REQUIRE(request.rtsp_version_minor == 0);
        REQUIRE(request.headers.empty());
        REQUIRE(request.data.empty());
    }
}

TEST_CASE("rtsp_request | encode", "[rtsp_request]") {
    rav::rtsp_request req;
    req.rtsp_version_major = 1;
    req.rtsp_version_minor = 0;
    req.method = "OPTIONS";
    req.uri = "*";
    req.headers.push_back({"CSeq", "1"});
    req.headers.push_back({"Accept", "application/sdp"});
    req.data = "Hello, World!";

    auto encoded = req.encode();
    REQUIRE(
        encoded == "OPTIONS * RTSP/1.0\r\nCSeq: 1\r\nAccept: application/sdp\r\ncontent-length: 13\r\n\r\nHello, World!"
    );
}
