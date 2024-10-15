/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/rtsp_response_parser.hpp"

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

TEST_CASE(
    "rtsp_response_parser | Parse responses as stream"
    "[rtsp_response_parser]"
) {
    const auto sdp = std::string(
        "v=0\r\no=- 123456 1 IN IP4 192.168.0.1\r\ns=Sample Media Stream\r\nc=IN IP4 192.168.0.1\r\nt=0 0\r\nm=audio 8000 RTP/AVP 0\r\na=rtpmap:0 PCMU/8000\r\nm=video 9000 RTP/AVP 96\r\na=rtpmap:96 H264/90000"
    );
    const auto responses = fmt::format(
        "RTSP/1.0 200 OK\r\nCSeq: 2\r\nContent-Type: application/sdp\r\nContent-Length: {}\r\n\r\n{}RTSP/1.0 400 Bad Request\r\nCSeq: 4\r\nContent-Type: text/plain\r\nContent-Length: 22\r\n\r\nInvalid header format.",
        sdp.size(), sdp
    );

    rav::rtsp_response response;
    rav::rtsp_response_parser parser(response);

    // In the middle of Content-Length header
    auto [result, begin] = parser.parse(responses.begin(), responses.begin() + 65);
    REQUIRE(result == rav::rtsp_response_parser::result::indeterminate);
    REQUIRE(begin == responses.begin() + 65);

    // Exactly at the end of the first request, after \r\n\r\n
    auto [result2, begin2] = parser.parse(begin, responses.begin() + 80);
    REQUIRE(result2 == rav::rtsp_response_parser::result::indeterminate);
    REQUIRE(begin2 == responses.begin() + 80);

    // In the middle of the data
    auto [result3, begin3] = parser.parse(begin2, responses.begin() + 144);
    REQUIRE(result3 == rav::rtsp_response_parser::result::indeterminate);
    REQUIRE(begin3 == responses.begin() + 144);

    // In the 2nd Content-Type header
    auto [result4, begin4] = parser.parse(begin3, responses.begin() + 306);
    REQUIRE(result4 == rav::rtsp_response_parser::result::good);
    REQUIRE(begin4 == responses.begin() + 263);

    // At this point the first response is complete, check the values:
    REQUIRE(response.rtsp_version_major == 1);
    REQUIRE(response.rtsp_version_minor == 0);
    REQUIRE(response.status_code == 200);
    REQUIRE(response.reason_phrase == "OK");
    REQUIRE(response.headers.size() == 3);
    REQUIRE(response.headers["CSeq"] == "2");
    REQUIRE(response.headers["Content-Type"] == "application/sdp");
    REQUIRE(response.headers["Content-Length"] == "183");
    REQUIRE(response.data.size() == sdp.size());
    REQUIRE(response.data == sdp);

    // Now onto the 2nd response
    parser.reset();

    // In the 2nd uri
    auto [result5, begin5] = parser.parse(begin4, responses.begin() + 306);
    REQUIRE(result5 == rav::rtsp_response_parser::result::indeterminate);
    REQUIRE(begin5 == responses.begin() + 306);

    auto [result6, begin6] = parser.parse(begin5, responses.end());
    REQUIRE(result6 == rav::rtsp_response_parser::result::good);
    REQUIRE(begin6 == responses.end());

    REQUIRE(response.rtsp_version_major == 1);
    REQUIRE(response.rtsp_version_minor == 0);
    REQUIRE(response.status_code == 400);
    REQUIRE(response.reason_phrase == "Bad Request");
    REQUIRE(response.headers.size() == 3);
    REQUIRE(response.headers["CSeq"] == "4");
    REQUIRE(response.headers["Content-Type"] == "text/plain");
    REQUIRE(response.headers["Content-Length"] == "22");
    REQUIRE(response.data == "Invalid header format.");
}

TEST_CASE("rtsp_response_parser | Parse ook response without data", "[rtsp_response_parser]") {
    const std::string response_text(
        "RTSP/1.0 200 OK\r\nCSeq: 3\r\nTransport: RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001\r\nSession: 12345678\r\nContent-Length: 0\r\n\r\n"
    );

    for (auto& txt : replace_newlines(response_text)) {
        rav::rtsp_response response;
        rav::rtsp_response_parser parser(response);

        auto [result, begin] = parser.parse(txt.begin(), txt.end());
        REQUIRE(result == rav::rtsp_response_parser::result::good);
        REQUIRE(begin == txt.end());
        REQUIRE(response.rtsp_version_major == 1);
        REQUIRE(response.rtsp_version_minor == 0);
        REQUIRE(response.status_code == 200);
        REQUIRE(response.reason_phrase == "OK");
        REQUIRE(response.headers.size() == 4);
        REQUIRE(response.headers["CSeq"] == "3");
        REQUIRE(response.headers["Transport"] == "RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001");
        REQUIRE(response.headers["Session"] == "12345678");
        REQUIRE(response.headers["Content-Length"] == "0");
        REQUIRE(response.data.empty());
    }
}

TEST_CASE("rtsp_response_parser | Parse ook response with data", "[rtsp_response_parser]") {
    const std::string response_text(
        "RTSP/1.0 200 OK\r\nCSeq: 3\r\nTransport: RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001\r\nSession: 12345678\r\nContent-Length: 18\r\n\r\nrtsp_response_data"
    );

    for (auto& txt : replace_newlines(response_text)) {
        rav::rtsp_response response;
        rav::rtsp_response_parser parser(response);

        auto [result, begin] = parser.parse(txt.begin(), txt.end());
        REQUIRE(result == rav::rtsp_response_parser::result::good);
        REQUIRE(begin == txt.end());
        REQUIRE(response.rtsp_version_major == 1);
        REQUIRE(response.rtsp_version_minor == 0);
        REQUIRE(response.status_code == 200);
        REQUIRE(response.reason_phrase == "OK");
        REQUIRE(response.headers.size() == 4);
        REQUIRE(response.headers["CSeq"] == "3");
        REQUIRE(response.headers["Transport"] == "RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001");
        REQUIRE(response.headers["Session"] == "12345678");
        REQUIRE(response.headers["Content-Length"] == "18");
        REQUIRE(response.data.size() == 18);
        REQUIRE(response.data == "rtsp_response_data");
    }
}
