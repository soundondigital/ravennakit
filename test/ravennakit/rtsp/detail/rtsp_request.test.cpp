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

#include "ravennakit/rtsp/detail/rtsp_request.hpp"

TEST_CASE("rtsp_request", "[rtsp_request]") {
    SECTION("Get header") {
        rav::rtsp::Request Request;
        request.rtsp_headers.push_back(rav::rtsp::Headers::Header {"Content-Length", "123"});
        request.rtsp_headers.push_back({"Content-Type", "application/sdp"});

        if (const auto* header = request.rtsp_headers.get("Content-Length"); header) {
            REQUIRE(header->value == "123");
        } else {
            FAIL("Content-Length header not found");
        }

        if (const auto* header = request.rtsp_headers.get("Content-Type"); header) {
            REQUIRE(header->value == "application/sdp");
        } else {
            FAIL("Content-Type header not found");
        }

        REQUIRE(request.rtsp_headers.get("Content-Size") == nullptr);
    }

    SECTION("Get content length") {
        rav::rtsp::Request Request;
        request.rtsp_headers.push_back({"Content-Length", "123"});

        if (auto content_length = request.rtsp_headers.get_content_length(); content_length) {
            REQUIRE(*content_length == 123);
        } else {
            FAIL("Content-Length header not found");
        }
    }

    SECTION("Get content length while there is no Content-Length header") {
        rav::rtsp::Request Request;
        REQUIRE(request.rtsp_headers.get_content_length() == std::nullopt);
    }

    SECTION("reset") {
        rav::rtsp::Request Request;
        request.method = "GET";
        request.uri = "/index.html";
        request.rtsp_version_major = 1;
        request.rtsp_version_minor = 1;
        request.rtsp_headers.emplace_back({"CSeq", "1"});
        request.data = "Hello, World!";
        request.reset();
        REQUIRE(request.method.empty());
        REQUIRE(request.uri.empty());
        REQUIRE(request.rtsp_version_major == 0);
        REQUIRE(request.rtsp_version_minor == 0);
        REQUIRE(request.rtsp_headers.empty());
        REQUIRE(request.data.empty());
    }
}

TEST_CASE("rtsp_request | encode", "[rtsp_request]") {
    rav::rtsp::Request req;
    req.rtsp_version_major = 1;
    req.rtsp_version_minor = 0;
    req.method = "OPTIONS";
    req.uri = "*";
    req.rtsp_headers.push_back({"CSeq", "1"});
    req.rtsp_headers.push_back({"Accept", "application/sdp"});
    req.data = "Hello, World!";

    auto encoded = req.encode();
    REQUIRE(
        encoded == "OPTIONS * RTSP/1.0\r\nCSeq: 1\r\nAccept: application/sdp\r\ncontent-length: 13\r\n\r\nHello, World!"
    );
}
