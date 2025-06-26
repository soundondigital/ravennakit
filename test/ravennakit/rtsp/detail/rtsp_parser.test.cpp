/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/detail/rtsp_parser.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::rtsp::Parser") {
    SECTION("parse responses in multiple chunks", "[rtsp_parser]") {
        const auto sdp = std::string(
            "v=0\r\no=- 123456 1 IN IP4 192.168.0.1\r\ns=Sample Media Stream\r\nc=IN IP4 192.168.0.1\r\nt=0 0\r\nm=audio 8000 RTP/AVP 0\r\na=rtpmap:0 PCMU/8000\r\nm=video 9000 RTP/AVP 96\r\na=rtpmap:96 H264/90000"
        );

        rav::StringBuffer input;
        rav::rtsp::Parser parser;

        int response_count = 0;

        parser.on_response = [&](const rav::rtsp::Response& response) {
            REQUIRE(response.rtsp_version_major == 1);
            REQUIRE(response.rtsp_version_minor == 0);
            REQUIRE(response.status_code == 200);
            REQUIRE(response.reason_phrase == "OK");
            REQUIRE(response.rtsp_headers.size() == 3);
            REQUIRE(response.rtsp_headers.get_or_default("CSeq") == "2");
            REQUIRE(response.rtsp_headers.get_or_default("Content-Type") == "application/sdp");
            REQUIRE(response.rtsp_headers.get_or_default("Content-Length") == std::to_string(sdp.size()));
            REQUIRE(response.data == sdp);
            response_count++;
        };

        input.write("RTSP/1.0 200 OK\r\nCSeq: 2\r\nContent-Type: application/sdp\r\nContent-");
        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::indeterminate);

        input.write(fmt::format("Length: {}\r\n\r\n", sdp.size()));
        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::indeterminate);

        input.write(sdp.substr(0, sdp.size() / 2));
        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::indeterminate);

        input.write(sdp.substr(sdp.size() / 2));
        input.write("RTSP/1.0 400 Bad Request\r\nCSeq: 4\r\nContent-");
        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::indeterminate);

        REQUIRE(response_count == 1);

        parser.on_response = [&](const rav::rtsp::Response& response) {
            REQUIRE(response.rtsp_version_major == 1);
            REQUIRE(response.rtsp_version_minor == 0);
            REQUIRE(response.status_code == 400);
            REQUIRE(response.reason_phrase == "Bad Request");
            REQUIRE(response.rtsp_headers.size() == 3);
            REQUIRE(response.rtsp_headers.get_or_default("CSeq") == "4");
            REQUIRE(response.rtsp_headers.get_or_default("Content-Type") == "text/plain");
            REQUIRE(response.rtsp_headers.get_or_default("Content-Length") == "22");
            REQUIRE(response.data == "Invalid header format.");
            response_count++;
        };

        input.write("Type: text/plain\r\nContent-Length: 22\r\n\r\nInvalid header format.");
        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::good);

        REQUIRE(response_count == 2);
    }

    SECTION("Parse ok response without data", "[rtsp_parser]") {
        const std::string response_text(
            "RTSP/1.0 200 OK\r\nCSeq: 3\r\nTransport: RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001\r\nSession: 12345678\r\nContent-Length: 0\r\n\r\n"
        );

        rav::StringBuffer input(response_text);
        input.write(rav::string_replace(response_text, "\r\n", "\n"));

        int response_count = 0;

        rav::rtsp::Parser parser;
        parser.on_response = [&](const rav::rtsp::Response& response) {
            REQUIRE(response.rtsp_version_major == 1);
            REQUIRE(response.rtsp_version_minor == 0);
            REQUIRE(response.status_code == 200);
            REQUIRE(response.reason_phrase == "OK");
            REQUIRE(response.rtsp_headers.size() == 4);
            REQUIRE(response.rtsp_headers.get_or_default("CSeq") == "3");
            REQUIRE(
                response.rtsp_headers.get_or_default("Transport")
                == "RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001"
            );
            REQUIRE(response.rtsp_headers.get_or_default("Session") == "12345678");
            REQUIRE(response.rtsp_headers.get_or_default("Content-Length") == "0");
            REQUIRE(response.data.empty());
            response_count++;
        };

        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::good);
        REQUIRE(response_count == 2);
    }

    SECTION("Parse ok response with data", "[rtsp_parser]") {
        const std::string response_text(
            "RTSP/1.0 200 OK\r\nCSeq: 3\r\nTransport: RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001\r\nSession: 12345678\r\nContent-Length: 18\r\n\r\nrtsp_response_data"
        );

        rav::StringBuffer input(response_text);
        input.write(rav::string_replace(response_text, "\r\n", "\n"));

        int response_count = 0;

        rav::rtsp::Parser parser;
        parser.on_response = [&](const rav::rtsp::Response& response) {
            REQUIRE(response.rtsp_version_major == 1);
            REQUIRE(response.rtsp_version_minor == 0);
            REQUIRE(response.status_code == 200);
            REQUIRE(response.reason_phrase == "OK");
            REQUIRE(response.rtsp_headers.size() == 4);
            REQUIRE(response.rtsp_headers.get_or_default("CSeq") == "3");
            REQUIRE(
                response.rtsp_headers.get_or_default("Transport")
                == "RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001"
            );
            REQUIRE(response.rtsp_headers.get_or_default("Session") == "12345678");
            REQUIRE(response.rtsp_headers.get_or_default("Content-Length") == "18");
            REQUIRE(response.data.size() == 18);
            REQUIRE(response.data == "rtsp_response_data");
            response_count++;
        };

        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::good);
        REQUIRE(response_count == 2);
    }

    SECTION("Parse response from Anubis", "[rtsp_parser]") {
        const std::string data(
            "v=0\r\no=- 13 0 IN IP4 192.168.15.52\r\ns=Anubis Combo LR\r\nc=IN IP4 239.1.15.52/15\r\nt=0 0\r\na=clock-domain:PTPv2 0\r\na=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\na=mediaclk:direct=0\r\nm=audio 5004 RTP/AVP 98\r\nc=IN IP4 239.1.15.52/15\r\na=rtpmap:98 L16/48000/2\r\na=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\na=clock-domain:PTPv2 0\r\na=sync-time:0\r\na=framecount:48\r\na=palign:0\r\na=ptime:1\r\na=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\na=mediaclk:direct=0\r\na=recvonly\r\na=midi-pre2:50040 0,0;0,1\r\n"
        );
        rav::StringBuffer input(
            "RTSP/1.0 200 OK\r\ncontent-length: 516\r\ncontent-type: application/sdp; charset=utf-8\r\n\r\n"
        );
        input.write(data);

        int response_count = 0;

        rav::rtsp::Parser parser;
        parser.on_response = [&](const rav::rtsp::Response& response) {
            REQUIRE(response.rtsp_version_major == 1);
            REQUIRE(response.rtsp_version_minor == 0);
            REQUIRE(response.status_code == 200);
            REQUIRE(response.reason_phrase == "OK");
            REQUIRE(response.rtsp_headers.size() == 2);
            REQUIRE(response.rtsp_headers.get_or_default("content-length") == "516");
            REQUIRE(response.rtsp_headers.get_or_default("content-type") == "application/sdp; charset=utf-8");
            REQUIRE(response.data.size() == data.size());
            REQUIRE(response.data == data);
            response_count++;
        };

        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::good);
        REQUIRE(response_count == 1);
    }

    SECTION("Parse some requests", "[rtsp_parser]") {
        SECTION("Parse without headers and without data") {
            constexpr auto txt = "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\n\r\n";
            rav::StringBuffer input(txt);
            input.write(rav::string_replace(txt, "\r\n", "\n"));

            int request_count = 0;

            rav::rtsp::Parser parser;
            parser.on_request = [&](const rav::rtsp::Request& request) {
                REQUIRE(request.method == "DESCRIBE");
                REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
                REQUIRE(request.rtsp_version_major == 1);
                REQUIRE(request.rtsp_version_minor == 0);
                REQUIRE(request.rtsp_headers.empty());
                REQUIRE(request.data.empty());
                request_count++;
            };

            REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::good);
            REQUIRE(request_count == 2);
        }

        SECTION("Parse with headers and without data") {
            constexpr auto txt =
                "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, application/rtsl, application/mheg\r\n\r\n";
            rav::StringBuffer input(txt);
            input.write(rav::string_replace(txt, "\r\n", "\n"));

            int request_count = 0;

            rav::rtsp::Parser parser;
            parser.on_request = [&](const rav::rtsp::Request& request) {
                REQUIRE(request.method == "DESCRIBE");
                REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
                REQUIRE(request.rtsp_version_major == 1);
                REQUIRE(request.rtsp_version_minor == 0);
                REQUIRE(request.rtsp_headers.size() == 2);
                REQUIRE(request.rtsp_headers.get_or_default("CSeq") == "312");
                REQUIRE(
                    request.rtsp_headers.get_or_default("Accept")
                    == "application/sdp, application/rtsl, application/mheg"
                );
                REQUIRE(request.data.empty());
                request_count++;
            };

            REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::good);
            REQUIRE(request_count == 2);
        }

        SECTION("Parse with headers and with data") {
            constexpr auto txt =
                "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nContent-Length: 28\r\n\r\nthis_is_the_part_called_data";
            rav::StringBuffer input(txt);
            input.write(rav::string_replace(txt, "\r\n", "\n"));

            int request_count = 0;

            rav::rtsp::Parser parser;
            parser.on_request = [&](const rav::rtsp::Request& request) {
                REQUIRE(request.method == "DESCRIBE");
                REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
                REQUIRE(request.rtsp_version_major == 1);
                REQUIRE(request.rtsp_version_minor == 0);
                REQUIRE(request.rtsp_headers.size() == 1);
                if (auto length = request.rtsp_headers.get_content_length()) {
                    REQUIRE(length.value() == 28);
                } else {
                    FAIL("Content-Length header not found");
                }
                REQUIRE(request.data == "this_is_the_part_called_data");
                request_count++;
            };

            REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::good);
            REQUIRE(request_count == 2);
        }

        SECTION("Parse folded headers") {
            auto space_folded =
                "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, \r\n application/rtsl, application/mheg\r\n\r\n";
            auto tab_folded =
                "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, \r\n\tapplication/rtsl, application/mheg\r\n\r\n";

            rav::StringBuffer input;
            input.write(space_folded);
            input.write(rav::string_replace(space_folded, "\r\n", "\n"));
            input.write(tab_folded);
            input.write(rav::string_replace(tab_folded, "\r\n", "\n"));

            int request_count = 0;

            rav::rtsp::Parser parser;
            parser.on_request = [&](const rav::rtsp::Request& request) {
                REQUIRE(request.method == "DESCRIBE");
                REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
                REQUIRE(request.rtsp_version_major == 1);
                REQUIRE(request.rtsp_version_minor == 0);
                REQUIRE(request.rtsp_headers.size() == 2);
                REQUIRE(request.rtsp_headers.get_or_default("CSeq") == "312");
                REQUIRE(
                    request.rtsp_headers.get_or_default("Accept")
                    == "application/sdp, application/rtsl, application/mheg"
                );
                REQUIRE(request.data.empty());
                request_count++;
            };

            REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::good);
            REQUIRE(request_count == 4);
        }
    }

    SECTION("Parse some requests in chunks", "[rtsp_parser]") {
        int request_count = 0;

        rav::rtsp::Parser parser;
        parser.on_request = [&](const rav::rtsp::Request& request) {
            REQUIRE(request.method == "DESCRIBE");
            REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE(request.rtsp_headers.size() == 1);
            REQUIRE(request.rtsp_headers.get_or_default("Content-Length") == "28");
            REQUIRE(request.data == "this_is_the_part_called_data");
            request_count++;
        };

        rav::StringBuffer input("DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nContent");
        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::indeterminate);

        input.write("-Length: 28\r\n\r\n");
        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::indeterminate);

        input.write("this_is_the_part");
        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::indeterminate);

        input.write("_called_dataOPTIONS rtsp://server2.example");
        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::indeterminate);

        REQUIRE(request_count == 1);

        parser.on_request = [&](const rav::rtsp::Request& request) {
            REQUIRE(request.method == "OPTIONS");
            REQUIRE(request.uri == "rtsp://server2.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE(request.rtsp_headers.size() == 1);
            REQUIRE(request.rtsp_headers.get_or_default("Content-Length") == "5");
            REQUIRE(request.data == "data2");
            request_count++;
        };

        input.write(".com/fizzle/foo RTSP/1.0\r\nContent-Length: 5\r\n\r\ndata2");
        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::good);

        REQUIRE(request_count == 2);
    }

    SECTION("Parse Anubis ANNOUNCE request", "[rtsp_parser]") {
        constexpr std::string_view sdp(
            "v=0\r\no=- 13 0 IN IP4 192.168.15.52\r\ns=Anubis Combo LR\r\nc=IN IP4 239.1.15.52/15\r\nt=0 0\r\na=clock-domain:PTPv2 0\r\na=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\na=mediaclk:direct=0\r\nm=audio 5004 RTP/AVP 98\r\nc=IN IP4 239.1.15.52/15\r\na=rtpmap:98 L16/48000/2\r\na=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\na=clock-domain:PTPv2 0\r\na=sync-time:0\r\na=framecount:48\r\na=palign:0\r\na=ptime:1\r\na=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\na=mediaclk:direct=0\r\na=recvonly\r\na=midi-pre2:50040 0,0;0,1\r\n"
        );

        rav::StringBuffer input("ANNOUNCE  RTSP/1.0\r\nconnection: Keep-Alive\r\ncontent-length: 516\r\n\r\n");
        input.write(sdp);

        int request_count = 0;

        rav::rtsp::Parser parser;
        parser.on_request = [&](const rav::rtsp::Request& request) {
            REQUIRE(request.method == "ANNOUNCE");
            REQUIRE(request.uri.empty());
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE(request.rtsp_headers.size() == 2);
            REQUIRE(request.rtsp_headers.get_or_default("content-length") == "516");
            REQUIRE(request.rtsp_headers.get_or_default("connection") == "Keep-Alive");
            REQUIRE(request.data == sdp);
            request_count++;
        };

        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::good);
        REQUIRE(request_count == 1);
    }

    SECTION("Parse Anubis DESCRIBE response and ANNOUNCE request", "[rtsp_parser]") {
        constexpr std::string_view sdp(
            "v=0\r\no=- 13 0 IN IP4 192.168.16.51\r\ns=Anubis Combo LR\r\nc=IN IP4 239.1.15.52/15\r\nt=0 0\r\na=clock-domain:PTPv2 0\r\na=ts-refclk:ptp=IEEE1588-2008:30-D6-59-FF-FE-01-DB-72:0\r\na=mediaclk:direct=0\r\nm=audio 5004 RTP/AVP 98\r\nc=IN IP4 239.1.15.52/15\r\na=rtpmap:98 L16/48000/2\r\na=source-filter: incl IN IP4 239.1.15.52 192.168.16.51\r\na=clock-domain:PTPv2 0\r\na=sync-time:0\r\na=framecount:48\r\na=palign:0\r\na=ptime:1\r\na=ts-refclk:ptp=IEEE1588-2008:30-D6-59-FF-FE-01-DB-72:0\r\na=mediaclk:direct=0\r\na=recvonly\r\na=midi-pre2:50040 0,0;0,1\r\n"
        );

        rav::StringBuffer input;
        input.write("RTSP/1.0 200 OK\r\ncontent-type: application/sdp; charset=utf-8\r\ncontent-length: 516\r\n\r\n");
        input.write(sdp);
        input.write("ANNOUNCE  RTSP/1.0\r\nconnection: Keep-Alive\r\ncontent-length: 516\r\n\r\n");
        input.write(sdp);

        int request_count = 0;
        int response_count = 0;

        rav::rtsp::Parser parser;

        parser.on_response = [&](const rav::rtsp::Response& response) {
            REQUIRE(response.rtsp_version_major == 1);
            REQUIRE(response.rtsp_version_minor == 0);
            REQUIRE(response.status_code == 200);
            REQUIRE(response.reason_phrase == "OK");
            REQUIRE(response.rtsp_headers.size() == 2);
            REQUIRE(response.rtsp_headers.get_or_default("content-length") == "516");
            REQUIRE(response.rtsp_headers.get_or_default("content-type") == "application/sdp; charset=utf-8");
            REQUIRE(response.data.size() == sdp.size());
            REQUIRE(response.data == sdp);
            response_count++;
        };

        parser.on_request = [&](const rav::rtsp::Request& request) {
            REQUIRE(request.method == "ANNOUNCE");
            REQUIRE(request.uri.empty());
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE(request.rtsp_headers.size() == 2);
            REQUIRE(request.rtsp_headers.get_or_default("content-length") == "516");
            REQUIRE(request.rtsp_headers.get_or_default("connection") == "Keep-Alive");
            REQUIRE(request.data == sdp);
            request_count++;
        };

        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::good);
        REQUIRE(request_count == 1);
        REQUIRE(response_count == 1);
    }

    SECTION("Parse special case from mic8") {
        constexpr std::string_view payload(
            "RTSP/1.0 200 OK\r\n"
            "cseq: 3\r\n"
            "\r\n"
            "RTSP/1.0 200 OK\r\n"
            "content-length: 473\r\n"
            "content-type: application/sdp; charset=utf-8\r\n"
            "cseq: 4\r\n"
            "\r\n"
            "v=0\r\n"
            "o=- 1744662004692777 5 IN IP4 192.168.15.55\r\n"
            "s=mic8-12\r\n"
            "t=0 0\r\n"
            "a=clock-domain:PTPv2 0\r\n"
            "a=sync-time:0\r\n"
            "a=ts-refclk:ptp=IEEE1588-2008:00-0B-72-FF-FE-07-DB-E6:0\r\n"
            "a=mediaclk:direct=0\r\n"
            "m=audio 5004 RTP/AVP 98\r\n"
            "c=IN IP4 239.15.55.1/31\r\n"
            "a=source-filter: incl IN IP4 239.15.55.1 192.168.15.55\r\n"
            "a=recvonly\r\n"
            "a=rtpmap:98 L24/48000/2\r\n"
            "a=framecount:48\r\n"
            "a=ptime:1\r\n"
            "a=clock-domain:PTPv2 0\r\n"
            "a=sync-time:0"
        );

        rav::StringBuffer input(payload.data());
        rav::rtsp::Parser parser;

        int request_count = 0;
        int response_count = 0;

        parser.on_response = [&](const rav::rtsp::Response& response) {
            REQUIRE(response.rtsp_version_major == 1);
            REQUIRE(response.rtsp_version_minor == 0);
            REQUIRE(response.status_code == 200);
            REQUIRE(response.reason_phrase == "OK");
            REQUIRE(response.rtsp_headers.size() == 1);
            REQUIRE(response.rtsp_headers.get_or_default("cseq") == "3");
            REQUIRE(response.data.empty());
            response_count++;
        };

        parser.on_request = [&](const rav::rtsp::Request&) {
            request_count++;
        };

        REQUIRE(parser.parse(input) == rav::rtsp::Parser::result::indeterminate);
    }
}
