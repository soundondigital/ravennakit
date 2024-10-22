/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/containers/string_buffer.hpp"
#include "ravennakit/rtsp/rtsp_client.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("string_buffer | prepare and commit, read and consume", "[string_buffer]") {
    constexpr std::string_view test_data = "0123456789";
    rav::string_buffer stream;

    // Prepare and commit
    REQUIRE(stream.remaining() == 0);
    auto buffer = stream.prepare(test_data.size());
    REQUIRE(stream.remaining() == 0);
    std::memcpy(buffer.data(), test_data.data(), test_data.size());
    stream.commit(test_data.size());
    REQUIRE(stream.remaining() == test_data.size());

    // Read and consume
    auto data = stream.data();
    REQUIRE(data.size() == test_data.size());
    REQUIRE(std::memcmp(data.data(), test_data.data(), test_data.size()) == 0);
    stream.consume(5);
    REQUIRE(stream.remaining() == test_data.size() - 5);
    auto read1 = stream.read(5);
    REQUIRE(read1.size() == 5);
    REQUIRE(std::memcmp(read1.data(), test_data.data() + 5, 5) == 0);
    REQUIRE(stream.exhausted());
}

TEST_CASE("string_buffer | read until newline", "[string_buffer]") {
    SECTION("LF") {
        rav::string_buffer stream;
        stream.write("Hello\nWorld\n\n");
        auto line1 = stream.read_until_newline();
        REQUIRE(line1);
        REQUIRE(*line1 == "Hello");
        REQUIRE(stream.remaining() == 7);
        auto line2 = stream.read_until_newline();
        REQUIRE(line2);
        REQUIRE(*line2 == "World");
        REQUIRE(stream.remaining() == 1);
        auto line3 = stream.read_until_newline();
        REQUIRE(line3);
        REQUIRE(line3->empty());
        REQUIRE(stream.exhausted());
        auto line4 = stream.read_until_newline();
        REQUIRE(!line4);
    }

    SECTION("CRLF") {
        rav::string_buffer stream;
        stream.write("Hello\r\nWorld\r\n\r\n");
        auto line1 = stream.read_until_newline();
        REQUIRE(line1);
        REQUIRE(*line1 == "Hello");
        REQUIRE(stream.remaining() == 9);
        auto line2 = stream.read_until_newline();
        REQUIRE(line2);
        REQUIRE(*line2 == "World");
        REQUIRE(stream.remaining() == 2);
        auto line3 = stream.read_until_newline();
        REQUIRE(line3);
        REQUIRE(line3->empty());
        REQUIRE(stream.exhausted());
        auto line4 = stream.read_until_newline();
        REQUIRE(!line4);
    }
}

TEST_CASE("string_buffer | reset", "[string_buffer]") {
    rav::string_buffer stream;
    stream.write("test");
    REQUIRE(stream.remaining() == 4);
    stream.clear();
    REQUIRE(stream.exhausted());
}

TEST_CASE("string_buffer | starts with", "[string_buffer]") {
    rav::string_buffer stream;
    stream.write("Hello World");
    REQUIRE(stream.starts_with("Hello"));
    REQUIRE(stream.starts_with("Hello World"));
    REQUIRE(!stream.starts_with("Hello World!"));
}
