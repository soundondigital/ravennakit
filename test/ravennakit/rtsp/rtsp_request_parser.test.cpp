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
 * Returns a vector with a copy of the original, and a copy with \r\n replaced with \n.
 * @param original The original string.
 * @return Vector of strings.
 */
std::vector<std::string> replace_newlines(const std::string& original) {
    std::vector<std::string> texts;
    texts.emplace_back(original);
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
            REQUIRE(begin == txt.end());
            REQUIRE(request.method == "DESCRIBE");
            REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE(request.headers.empty());
        }
    }

    SECTION("Parse with headers and without data") {
        auto texts = replace_newlines(
            "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, application/rtsl, application/mheg\r\n\r\n"
        );

        for (auto& txt : texts) {
            rav::rtsp_request request;
            rav::rtsp_request_parser parser(request);
            auto [result, begin] = parser.parse(txt.begin(), txt.end());
            REQUIRE(result == rav::rtsp_request_parser::result::good);
            REQUIRE(begin == txt.end());
            REQUIRE(request.method == "DESCRIBE");
            REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE_FALSE(request.headers.empty());
            REQUIRE(request.headers.size() == 2);
            REQUIRE(request.headers[0].name == "CSeq");
            REQUIRE(request.headers[0].value == "312");
            REQUIRE(request.headers[1].name == "Accept");
            REQUIRE(request.headers[1].value == "application/sdp, application/rtsl, application/mheg");
        }
    }

    SECTION("Parse folded headers") {
        auto texts = replace_newlines(
            "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, \r\n application/rtsl, application/mheg\r\n\r\n"
        );

        auto tab_folder = replace_newlines(
            "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, \r\n\tapplication/rtsl, application/mheg\r\n\r\n"
        );

        texts.insert(texts.end(), tab_folder.begin(), tab_folder.end());

        for (auto& txt : texts) {
            rav::rtsp_request request;
            rav::rtsp_request_parser parser(request);
            auto [result, begin] = parser.parse(txt.begin(), txt.end());
            REQUIRE(result == rav::rtsp_request_parser::result::good);
            REQUIRE(begin == txt.end());
            REQUIRE(request.method == "DESCRIBE");
            REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE_FALSE(request.headers.empty());
            REQUIRE(request.headers.size() == 2);
            REQUIRE(request.headers[0].name == "CSeq");
            REQUIRE(request.headers[0].value == "312");
            REQUIRE(request.headers[1].name == "Accept");
            REQUIRE(request.headers[1].value == "application/sdp, application/rtsl, application/mheg");
        }
    }

    SECTION("Parse chunked") {
        auto texts = replace_newlines(
            "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, application/rtsl, application/mheg\r\n\r\n"
        );

        for (auto& txt : texts) {
            rav::rtsp_request request;
            rav::rtsp_request_parser parser(request);
            constexpr size_t chunk_size = 4;
            for (size_t i = 0; i < txt.size(); i += chunk_size) {
                auto subview = txt.substr(i, chunk_size);
                auto [result, begin] = parser.parse(subview.begin(), subview.end());
                REQUIRE(begin == subview.end());
                if (result == rav::rtsp_request_parser::result::good) {
                    break;
                }
                REQUIRE(result == rav::rtsp_request_parser::result::indeterminate);
            }
            REQUIRE(request.method == "DESCRIBE");
            REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE_FALSE(request.headers.empty());
            REQUIRE(request.headers.size() == 2);
            REQUIRE(request.headers[0].name == "CSeq");
            REQUIRE(request.headers[0].value == "312");
            REQUIRE(request.headers[1].name == "Accept");
            REQUIRE(request.headers[1].value == "application/sdp, application/rtsl, application/mheg");
        }
    }

    SECTION("Parse with headers and with data") {
        auto texts = replace_newlines(
            "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nContent-Length: 28\r\n\r\nthis_is_the_part_called_data"
        );

        for (auto& txt : texts) {
            rav::rtsp_request request;
            rav::rtsp_request_parser parser(request);
            auto [result, begin] = parser.parse(txt.begin(), txt.end());
            REQUIRE(result == rav::rtsp_request_parser::result::good);
            REQUIRE(begin == txt.end());
            REQUIRE(request.method == "DESCRIBE");
            REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            if (auto length = request.headers.get_content_length()) {
                REQUIRE(length.value() == 28);
            } else {
                FAIL("Content-Length header not found");
            }
            REQUIRE(request.data == "this_is_the_part_called_data");
        }
    }
}

TEST_CASE("rtsp_request_parser | Parse in different chunks", "[rtsp_request_parser]") {
    constexpr std::string_view rtsp(
        "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nContent-Length: 28\r\n\r\nthis_is_the_part_called_dataOPTIONS rtsp://server2.example.com/fizzle/foo RTSP/1.0\r\nContent-Length: 5\r\n\r\ndata2"
    );

    rav::rtsp_request request;
    rav::rtsp_request_parser parser(request);

    // In the middle of Content-Length header
    auto [result, begin] = parser.parse(rtsp.begin(), rtsp.begin() + 64);
    REQUIRE(result == rav::rtsp_request_parser::result::indeterminate);
    REQUIRE(begin == rtsp.begin() + 64);

    // Exactly at the end of the first request, after \r\n\r\n
    auto [result2, begin2] = parser.parse(begin, rtsp.begin() + 78);
    REQUIRE(result2 == rav::rtsp_request_parser::result::indeterminate);
    REQUIRE(begin2 == rtsp.begin() + 78);

    // In the middle of the data
    auto [result3, begin3] = parser.parse(begin2, rtsp.begin() + 94);
    REQUIRE(result3 == rav::rtsp_request_parser::result::indeterminate);
    REQUIRE(begin3 == rtsp.begin() + 94);

    // In the 2nd uri
    auto [result4, begin4] = parser.parse(begin3, rtsp.begin() + 134);
    REQUIRE(result4 == rav::rtsp_request_parser::result::good);
    REQUIRE(begin4 == rtsp.begin() + 106);

    REQUIRE(request.method == "DESCRIBE");
    REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
    REQUIRE(request.rtsp_version_major == 1);
    REQUIRE(request.rtsp_version_minor == 0);
    if (auto length = request.headers.get_content_length(); length.has_value()) {
        REQUIRE(length.value() == 28);
    } else {
        FAIL("Content-Length header not found");
    }
    REQUIRE(request.data == "this_is_the_part_called_data");

    // At this point a full request has been parsed, and we can reset the parser.
    parser.reset();

    // In the 2nd uri
    auto [result5, begin5] = parser.parse(begin4, rtsp.begin() + 134);
    REQUIRE(result5 == rav::rtsp_request_parser::result::indeterminate);
    REQUIRE(begin5 == rtsp.begin() + 134);

    auto [result6, begin6] = parser.parse(begin5, rtsp.end());
    REQUIRE(result6 == rav::rtsp_request_parser::result::good);
    REQUIRE(begin6 == rtsp.end());

    REQUIRE(request.method == "OPTIONS");
    REQUIRE(request.uri == "rtsp://server2.example.com/fizzle/foo");
    REQUIRE(request.rtsp_version_major == 1);
    REQUIRE(request.rtsp_version_minor == 0);
    if (auto length = request.headers.get_content_length(); length.has_value()) {
        REQUIRE(length.value() == 5);
    } else {
        FAIL("Content-Length header not found");
    }
    REQUIRE(request.data == "data2");
}
