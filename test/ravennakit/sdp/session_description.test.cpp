/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/session_description.hpp"

#include <catch2/catch_all.hpp>

namespace {

constexpr auto k_anubis_sdp =
    "v=0\r\n"
    "o=- 13 0 IN IP4 192.168.15.52\r\n"
    "s=Anubis_610120_13\r\n"
    "c=IN IP4 239.1.15.52/15\r\n"
    "t=0 0\r\n"
    "a=clock-domain:PTPv2 0\r\n"
    "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
    "a=mediaclk:direct=0\r\n"
    "m=audio 5004 RTP/AVP 98\r\n"
    "c=IN IP4 239.1.15.52/15\r\n"
    "a=rtpmap:98 L16/48000/2\r\n"
    "a=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\n"
    "a=clock-domain:PTPv2 0\r\n"
    "a=sync-time:0\r\n"
    "a=framecount:48\r\n"
    "a=palign:0\r\n"
    "a=ptime:1\r\n"
    "a=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\n"
    "a=mediaclk:direct=0\r\n"
    "a=recvonly\r\n"
    "a=midi-pre2:50040 0,0;0,1\r\n";

}

TEST_CASE("session_description | test basic assumptions", "[session_description]") {
    SECTION("Test newline delimited string") {
        std::string text = "line1\nline2\nline3\n";
        std::istringstream stream(text);
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(stream, line)) {
            // Remove trailing '\r' if present (because of CRLF)
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }
        REQUIRE(lines.size() == 3);
        REQUIRE(lines[0] == "line1");
        REQUIRE(lines[1] == "line2");
        REQUIRE(lines[2] == "line3");
    }

    SECTION("Test crlf delimited string") {
        std::string text = "line1\r\nline2\r\nline3\r\n";
        std::istringstream stream(text);
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(stream, line)) {
            // Remove trailing '\r' if present (because of CRLF)
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }
        REQUIRE(lines.size() == 3);
        REQUIRE(lines[0] == "line1");
        REQUIRE(lines[1] == "line2");
        REQUIRE(lines[2] == "line3");
    }
}

TEST_CASE("session_description", "[session_description]") {
    SECTION("Parse a description from Anubis") {
        auto [result, sd] = rav::session_description::parse(k_anubis_sdp);
        REQUIRE(result == rav::session_description::parse_result::ok);
        REQUIRE(sd.version() == 0);
    }

    SECTION("Test crlf delimited string") {
        constexpr auto crlf =
            "v=0\r\n"
            "o=- 13 0 IN IP4 192.168.15.52\r\n"
            "s=Anubis_610120_13\r\n";
        auto sd = rav::session_description::parse(crlf);
        // TODO: Verify
    }

    SECTION("Test n delimited string") {
        constexpr auto crlf =
            "v=0\n"
            "o=- 13 0 IN IP4 192.168.15.52\n"
            "s=Anubis_610120_13\n";
        auto sd = rav::session_description::parse(crlf);
        // TODO: Verify
    }
}
