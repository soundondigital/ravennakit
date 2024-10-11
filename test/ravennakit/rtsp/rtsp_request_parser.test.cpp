/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/rtsp_request_parser.hpp"

#include <catch2/catch_all.hpp>

namespace {

/**
 * Returns a vector with copies of original, with the original itself, one with \r\n replaced with \n and one with \r\n
 * replaced with \r.
 * @param original The original string.
 * @return Vector of strings.
 */
std::vector<std::string> replace_newlines(const std::string& original) {
    std::vector<std::string> texts;
    texts.emplace_back(original);
    texts.emplace_back(rav::string_replace(original, "\r\n", "\r"));
    texts.emplace_back(rav::string_replace(original, "\r\n", "\n"));
    return texts;
}

}  // namespace

TEST_CASE("rtsp_request_parser", "[rtsp_request_parser]") {
    SECTION("Parse without headers and without data") {
        auto texts = replace_newlines("DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\n\r\n");

        for (auto& txt : texts) {
            rav::rtsp_request request;
            rav::rtsp_request_parser parser(request);
            auto [result, begin] = parser.parse(txt.begin(), txt.end());
            REQUIRE(result == rav::rtsp_request_parser::result::good);
            REQUIRE(request.method == "DESCRIBE");
            REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE(request.headers.empty());
        }
    }

    // SECTION("Parse with headers and without data") {
    //     auto texts = replace_newlines(
    //         "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, application/rtsl, application/mheg\r\n\r\n"
    //     );
    //
    //     for (auto& txt : texts) {
    //         rav::rtsp_request request;
    //         rav::rtsp_request_parser parser(request);
    //         auto [result, begin] = parser.parse(txt.begin(), txt.end());
    //         REQUIRE(result == rav::rtsp_request_parser::result::good);
    //         REQUIRE(request.method == "DESCRIBE");
    //         REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
    //         REQUIRE(request.rtsp_version_major == 1);
    //         REQUIRE(request.rtsp_version_minor == 0);
    //         REQUIRE_FALSE(request.headers.empty());
    //         REQUIRE(request.headers.size() == 2);
    //         REQUIRE(request.headers[0].name == "CSeq");
    //         REQUIRE(request.headers[0].value == "312");
    //         REQUIRE(request.headers[1].name == "Accept");
    //         REQUIRE(request.headers[1].value == "application/sdp, application/rtsl, application/mheg");
    //     }
    // }
    //
    //
    // SECTION("Parse chunked") {
    //     auto texts = replace_newlines(
    //         "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, application/rtsl, application/mheg\r\n\r\n"
    //     );
    //
    //     rav::rtsp_request request;
    //     rav::rtsp_request_parser parser(request);
    //
    //     for (auto& txt : texts) {
    //         constexpr size_t chunk_size = 3;
    //         for (size_t i = 0; i < txt.size(); i += chunk_size) {
    //             auto subview = txt.substr(i, chunk_size);
    //             auto [result, consumed] = parser.parse(subview.begin(), subview.end());
    //
    //             if (result == rav::rtsp_request_parser::result::good) {
    //                 REQUIRE(consumed == subview.end());
    //                 break;
    //             }
    //
    //             REQUIRE(result == rav::rtsp_request_parser::result::indeterminate);
    //         }
    //     }
    //
    //     REQUIRE(request.method == "DESCRIBE");
    // }
}
