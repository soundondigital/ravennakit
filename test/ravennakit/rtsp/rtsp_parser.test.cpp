/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/rtsp_parser.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rtsp_parser | parse responses in multiple chunks", "[rtsp_parser]") {
    const auto sdp = std::string(
        "v=0\r\no=- 123456 1 IN IP4 192.168.0.1\r\ns=Sample Media Stream\r\nc=IN IP4 192.168.0.1\r\nt=0 0\r\nm=audio 8000 RTP/AVP 0\r\na=rtpmap:0 PCMU/8000\r\nm=video 9000 RTP/AVP 96\r\na=rtpmap:96 H264/90000"
    );

    rav::string_buffer input;
    rav::rtsp_parser parser;

    int response_count = 0;

    parser.on<rav::rtsp_response>([&](const rav::rtsp_response& response, rav::rtsp_parser&) {
        REQUIRE(response.rtsp_version_major == 1);
        REQUIRE(response.rtsp_version_minor == 0);
        REQUIRE(response.status_code == 200);
        REQUIRE(response.reason_phrase == "OK");
        REQUIRE(response.headers.size() == 3);
        REQUIRE(response.headers["CSeq"] == "2");
        REQUIRE(response.headers["Content-Type"] == "application/sdp");
        REQUIRE(response.headers["Content-Length"] == std::to_string(sdp.size()));
        REQUIRE(response.data == sdp);
        response_count++;
    });

    input.write("RTSP/1.0 200 OK\r\nCSeq: 2\r\nContent-Type: application/sdp\r\nContent-");
    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::indeterminate);

    input.write(fmt::format("Length: {}\r\n\r\n", sdp.size()));
    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::indeterminate);

    input.write(sdp.substr(0, sdp.size() / 2));
    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::indeterminate);

    input.write(sdp.substr(sdp.size() / 2));
    input.write("RTSP/1.0 400 Bad Request\r\nCSeq: 4\r\nContent-");
    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::indeterminate);

    REQUIRE(response_count == 1);

    parser.on<rav::rtsp_response>([&](const rav::rtsp_response& response, rav::rtsp_parser&) {
        REQUIRE(response.rtsp_version_major == 1);
        REQUIRE(response.rtsp_version_minor == 0);
        REQUIRE(response.status_code == 400);
        REQUIRE(response.reason_phrase == "Bad Request");
        REQUIRE(response.headers.size() == 3);
        REQUIRE(response.headers["CSeq"] == "4");
        REQUIRE(response.headers["Content-Type"] == "text/plain");
        REQUIRE(response.headers["Content-Length"] == "22");
        REQUIRE(response.data == "Invalid header format.");
        response_count++;
    });

    input.write("Type: text/plain\r\nContent-Length: 22\r\n\r\nInvalid header format.");
    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::good);

    REQUIRE(response_count == 2);
}

TEST_CASE("rtsp_parser | Parse ok response without data", "[rtsp_parser]") {
    const std::string response_text(
        "RTSP/1.0 200 OK\r\nCSeq: 3\r\nTransport: RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001\r\nSession: 12345678\r\nContent-Length: 0\r\n\r\n"
    );

    rav::string_buffer input(response_text);
    input.write(rav::string_replace(response_text, "\r\n", "\n"));

    int response_count = 0;

    rav::rtsp_parser parser;
    parser.on<rav::rtsp_response>([&](const rav::rtsp_response& response, rav::rtsp_parser&) {
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
        response_count++;
    });

    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::good);
    REQUIRE(response_count == 2);
}

TEST_CASE("rtsp_parser | Parse ok response with data", "[rtsp_parser]") {
    const std::string response_text(
        "RTSP/1.0 200 OK\r\nCSeq: 3\r\nTransport: RTP/AVP;unicast;client_port=8000-8001;server_port=9000-9001\r\nSession: 12345678\r\nContent-Length: 18\r\n\r\nrtsp_response_data"
    );

    rav::string_buffer input(response_text);
    input.write(rav::string_replace(response_text, "\r\n", "\n"));

    int response_count = 0;

    rav::rtsp_parser parser;
    parser.on<rav::rtsp_response>([&](const rav::rtsp_response& response, rav::rtsp_parser&) {
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
        response_count++;
    });

    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::good);
    REQUIRE(response_count == 2);
}

TEST_CASE("rtsp_parser | Parse response from Anubis", "[rtsp_parser]") {
    const std::string data(
        "v=0\r\no=- 13 0 IN IP4 192.168.15.52\r\ns=Anubis Combo LR\r\nc=IN IP4 239.1.15.52/15\r\nt=0 0\r\na=clock-domain:PTPv2 0\r\na=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\na=mediaclk:direct=0\r\nm=audio 5004 RTP/AVP 98\r\nc=IN IP4 239.1.15.52/15\r\na=rtpmap:98 L16/48000/2\r\na=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\na=clock-domain:PTPv2 0\r\na=sync-time:0\r\na=framecount:48\r\na=palign:0\r\na=ptime:1\r\na=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\na=mediaclk:direct=0\r\na=recvonly\r\na=midi-pre2:50040 0,0;0,1\r\n"
    );
    rav::string_buffer input(
        "RTSP/1.0 200 OK\r\ncontent-length: 516\r\ncontent-type: application/sdp; charset=utf-8\r\n\r\n"
    );
    input.write(data);

    int response_count = 0;

    rav::rtsp_parser parser;
    parser.on<rav::rtsp_response>([&](const rav::rtsp_response& response, rav::rtsp_parser&) {
        REQUIRE(response.rtsp_version_major == 1);
        REQUIRE(response.rtsp_version_minor == 0);
        REQUIRE(response.status_code == 200);
        REQUIRE(response.reason_phrase == "OK");
        REQUIRE(response.headers.size() == 2);
        REQUIRE(response.headers["content-length"] == "516");
        REQUIRE(response.headers["content-type"] == "application/sdp; charset=utf-8");
        REQUIRE(response.data.size() == data.size());
        REQUIRE(response.data == data);
        response_count++;
    });

    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::good);
    REQUIRE(response_count == 1);
}

TEST_CASE("rtsp_parser | Parse some requests", "[rtsp_parser]") {
    SECTION("Parse without headers and without data") {
        constexpr auto txt = "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\n\r\n";
        rav::string_buffer input(txt);
        input.write(rav::string_replace(txt, "\r\n", "\n"));

        int request_count = 0;

        rav::rtsp_parser parser;
        parser.on<rav::rtsp_request>([&](const rav::rtsp_request& request, rav::rtsp_parser&) {
            REQUIRE(request.method == "DESCRIBE");
            REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE(request.headers.empty());
            REQUIRE(request.data.empty());
            request_count++;
        });

        REQUIRE(parser.parse(input) == rav::rtsp_parser::result::good);
        REQUIRE(request_count == 2);
    }

    SECTION("Parse with headers and without data") {
        constexpr auto txt =
            "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, application/rtsl, application/mheg\r\n\r\n";
        rav::string_buffer input(txt);
        input.write(rav::string_replace(txt, "\r\n", "\n"));

        int request_count = 0;

        rav::rtsp_parser parser;
        parser.on<rav::rtsp_request>([&](const rav::rtsp_request& request, rav::rtsp_parser&) {
            REQUIRE(request.method == "DESCRIBE");
            REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE(request.headers.size() == 2);
            REQUIRE(request.headers["CSeq"] == "312");
            REQUIRE(request.headers["Accept"] == "application/sdp, application/rtsl, application/mheg");
            REQUIRE(request.data.empty());
            request_count++;
        });

        REQUIRE(parser.parse(input) == rav::rtsp_parser::result::good);
        REQUIRE(request_count == 2);
    }

    SECTION("Parse with headers and with data") {
        constexpr auto txt =
            "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nContent-Length: 28\r\n\r\nthis_is_the_part_called_data";
        rav::string_buffer input(txt);
        input.write(rav::string_replace(txt, "\r\n", "\n"));

        int request_count = 0;

        rav::rtsp_parser parser;
        parser.on<rav::rtsp_request>([&](const rav::rtsp_request& request, rav::rtsp_parser&) {
            REQUIRE(request.method == "DESCRIBE");
            REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE(request.headers.size() == 1);
            if (auto length = request.headers.get_content_length()) {
                REQUIRE(length.value() == 28);
            } else {
                FAIL("Content-Length header not found");
            }
            REQUIRE(request.data == "this_is_the_part_called_data");
            request_count++;
        });

        REQUIRE(parser.parse(input) == rav::rtsp_parser::result::good);
        REQUIRE(request_count == 2);
    }

    SECTION("Parse folded headers") {
        auto space_folded =
            "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, \r\n application/rtsl, application/mheg\r\n\r\n";
        auto tab_folded =
            "DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nCSeq: 312\r\nAccept: application/sdp, \r\n\tapplication/rtsl, application/mheg\r\n\r\n";

        rav::string_buffer input;
        input.write(space_folded);
        input.write(rav::string_replace(space_folded, "\r\n", "\n"));
        input.write(tab_folded);
        input.write(rav::string_replace(tab_folded, "\r\n", "\n"));

        int request_count = 0;

        rav::rtsp_parser parser;
        parser.on<rav::rtsp_request>([&](const rav::rtsp_request& request, rav::rtsp_parser&) {
            REQUIRE(request.method == "DESCRIBE");
            REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
            REQUIRE(request.rtsp_version_major == 1);
            REQUIRE(request.rtsp_version_minor == 0);
            REQUIRE(request.headers.size() == 2);
            REQUIRE(request.headers["CSeq"] == "312");
            REQUIRE(request.headers["Accept"] == "application/sdp, application/rtsl, application/mheg");
            REQUIRE(request.data.empty());
            request_count++;
        });

        REQUIRE(parser.parse(input) == rav::rtsp_parser::result::good);
        REQUIRE(request_count == 4);
    }
}

TEST_CASE("rtsp_parser | Parse some requests in chunks", "[rtsp_parser]") {
    int request_count = 0;

    rav::rtsp_parser parser;
    parser.on<rav::rtsp_request>([&](const rav::rtsp_request& request, rav::rtsp_parser&) {
        REQUIRE(request.method == "DESCRIBE");
        REQUIRE(request.uri == "rtsp://server.example.com/fizzle/foo");
        REQUIRE(request.rtsp_version_major == 1);
        REQUIRE(request.rtsp_version_minor == 0);
        REQUIRE(request.headers.size() == 1);
        REQUIRE(request.headers["Content-Length"] == "28");
        REQUIRE(request.data == "this_is_the_part_called_data");
        request_count++;
    });

    rav::string_buffer input("DESCRIBE rtsp://server.example.com/fizzle/foo RTSP/1.0\r\nContent");
    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::indeterminate);

    input.write("-Length: 28\r\n\r\n");
    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::indeterminate);

    input.write("this_is_the_part");
    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::indeterminate);

    input.write("_called_dataOPTIONS rtsp://server2.example");
    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::indeterminate);

    REQUIRE(request_count == 1);

    parser.on<rav::rtsp_request>([&](const rav::rtsp_request& request, rav::rtsp_parser&) {
        REQUIRE(request.method == "OPTIONS");
        REQUIRE(request.uri == "rtsp://server2.example.com/fizzle/foo");
        REQUIRE(request.rtsp_version_major == 1);
        REQUIRE(request.rtsp_version_minor == 0);
        REQUIRE(request.headers.size() == 1);
        REQUIRE(request.headers["Content-Length"] == "5");
        REQUIRE(request.data == "data2");
        request_count++;
    });

    input.write(".com/fizzle/foo RTSP/1.0\r\nContent-Length: 5\r\n\r\ndata2");
    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::good);

    REQUIRE(request_count == 2);
}

TEST_CASE("rtsp_parser | Parse Anubis ANNOUNCE request", "[rtsp_parser]") {
    constexpr std::string_view sdp(
        "v=0\r\no=- 13 0 IN IP4 192.168.15.52\r\ns=Anubis Combo LR\r\nc=IN IP4 239.1.15.52/15\r\nt=0 0\r\na=clock-domain:PTPv2 0\r\na=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\na=mediaclk:direct=0\r\nm=audio 5004 RTP/AVP 98\r\nc=IN IP4 239.1.15.52/15\r\na=rtpmap:98 L16/48000/2\r\na=source-filter: incl IN IP4 239.1.15.52 192.168.15.52\r\na=clock-domain:PTPv2 0\r\na=sync-time:0\r\na=framecount:48\r\na=palign:0\r\na=ptime:1\r\na=ts-refclk:ptp=IEEE1588-2008:00-1D-C1-FF-FE-51-9E-F7:0\r\na=mediaclk:direct=0\r\na=recvonly\r\na=midi-pre2:50040 0,0;0,1\r\n"
    );

    rav::string_buffer input("ANNOUNCE  RTSP/1.0\r\nconnection: Keep-Alive\r\ncontent-length: 516\r\n\r\n");
    input.write(sdp);

    int request_count = 0;

    rav::rtsp_parser parser;
    parser.on<rav::rtsp_request>([&](const rav::rtsp_request& request, rav::rtsp_parser&) {
        REQUIRE(request.method == "ANNOUNCE");
        REQUIRE(request.uri.empty());
        REQUIRE(request.rtsp_version_major == 1);
        REQUIRE(request.rtsp_version_minor == 0);
        REQUIRE(request.headers.size() == 2);
        REQUIRE(request.headers["content-length"] == "516");
        REQUIRE(request.headers["connection"] == "Keep-Alive");
        REQUIRE(request.data == sdp);
        request_count++;
    });

    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::good);
    REQUIRE(request_count == 1);
}

TEST_CASE("rtsp_parser | Parse Anubis DESCRIBE response and ANNOUNCE request", "[rtsp_parser]") {
    constexpr std::string_view sdp(
        "v=0\r\no=- 13 0 IN IP4 192.168.16.51\r\ns=Anubis Combo LR\r\nc=IN IP4 239.1.15.52/15\r\nt=0 0\r\na=clock-domain:PTPv2 0\r\na=ts-refclk:ptp=IEEE1588-2008:30-D6-59-FF-FE-01-DB-72:0\r\na=mediaclk:direct=0\r\nm=audio 5004 RTP/AVP 98\r\nc=IN IP4 239.1.15.52/15\r\na=rtpmap:98 L16/48000/2\r\na=source-filter: incl IN IP4 239.1.15.52 192.168.16.51\r\na=clock-domain:PTPv2 0\r\na=sync-time:0\r\na=framecount:48\r\na=palign:0\r\na=ptime:1\r\na=ts-refclk:ptp=IEEE1588-2008:30-D6-59-FF-FE-01-DB-72:0\r\na=mediaclk:direct=0\r\na=recvonly\r\na=midi-pre2:50040 0,0;0,1\r\n"
    );

    rav::string_buffer input;
    input.write("RTSP/1.0 200 OK\r\ncontent-type: application/sdp; charset=utf-8\r\ncontent-length: 516\r\n\r\n");
    input.write(sdp);
    input.write("ANNOUNCE  RTSP/1.0\r\nconnection: Keep-Alive\r\ncontent-length: 516\r\n\r\n");
    input.write(sdp);

    int request_count = 0;
    int response_count = 0;

    rav::rtsp_parser parser;

    parser.on<rav::rtsp_response>([&](const rav::rtsp_response& response, rav::rtsp_parser&) {
        REQUIRE(response.rtsp_version_major == 1);
        REQUIRE(response.rtsp_version_minor == 0);
        REQUIRE(response.status_code == 200);
        REQUIRE(response.reason_phrase == "OK");
        REQUIRE(response.headers.size() == 2);
        REQUIRE(response.headers["content-length"] == "516");
        REQUIRE(response.headers["content-type"] == "application/sdp; charset=utf-8");
        REQUIRE(response.data.size() == sdp.size());
        REQUIRE(response.data == sdp);
        response_count++;
    });

    parser.on<rav::rtsp_request>([&](const rav::rtsp_request& request, rav::rtsp_parser&) {
        REQUIRE(request.method == "ANNOUNCE");
        REQUIRE(request.uri.empty());
        REQUIRE(request.rtsp_version_major == 1);
        REQUIRE(request.rtsp_version_minor == 0);
        REQUIRE(request.headers.size() == 2);
        REQUIRE(request.headers["content-length"] == "516");
        REQUIRE(request.headers["connection"] == "Keep-Alive");
        REQUIRE(request.data == sdp);
        request_count++;
    });

    REQUIRE(parser.parse(input) == rav::rtsp_parser::result::good);
    REQUIRE(request_count == 1);
    REQUIRE(response_count == 1);
}
