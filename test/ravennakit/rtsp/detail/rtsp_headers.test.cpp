/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/detail/rtsp_request.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::rtsp::Headers") {
    SECTION("Find header") {
        rav::rtsp::Headers headers;
        REQUIRE(headers.get("CSeq") == nullptr);
        headers.push_back({"CSeq", "1"});
        auto* header = headers.get("CSeq");
        REQUIRE(header != nullptr);
        REQUIRE(header->value == "1");
    }

    SECTION("Get content length") {
        rav::rtsp::Headers headers;
        REQUIRE(headers.get_content_length() == std::nullopt);
        headers.emplace_back({"Content-Length", "10"});
        REQUIRE(headers.get_content_length() == 10);
    }

    SECTION("reset") {
        rav::rtsp::Headers headers;
        headers.push_back({"CSeq", "1"});
        headers.push_back({"Content-Length", "10"});
        headers.clear();
        REQUIRE(headers.empty());
    }

    SECTION("Add header, make sure existing header gets updated using emplace_back") {
        rav::rtsp::Headers headers;
        headers.emplace_back({"CSeq", "1"});
        headers.emplace_back({"CSeq", "2"});
        REQUIRE(headers.size() == 1);
        REQUIRE(headers.get_or_default("CSeq") == "2");
    }

    SECTION("Add header, make sure existing header gets updated using emplace_back case insensitive") {
        rav::rtsp::Headers headers;
        headers.emplace_back({"cseq", "1"});
        headers.emplace_back({"CSeq", "2"});
        REQUIRE(headers.size() == 1);
        REQUIRE(headers[0].value == "2");
    }

    SECTION("Add header, make sure existing header gets updated using push_back") {
        rav::rtsp::Headers headers;
        headers.push_back({"CSeq", "1"});
        headers.push_back({"CSeq", "2"});
        REQUIRE(headers.size() == 1);
        REQUIRE(headers[0].value == "2");
    }

    SECTION("Add header, make sure existing header gets updated using push_back case insensitive") {
        rav::rtsp::Headers headers;
        headers.push_back({"cseq", "1"});
        headers.push_back({"CSeq", "2"});
        REQUIRE(headers.size() == 1);
        REQUIRE(headers[0].value == "2");
    }
}
