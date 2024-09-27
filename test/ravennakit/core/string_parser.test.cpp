/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/string_parser.hpp"

#include <catch2/catch_all.hpp>

#include "ravennakit/core/util.hpp"

TEST_CASE("string_parser", "[string_parser]") {
    SECTION("Test delimited string without include_delimiter") {
        const auto str = "this is just a random string";
        rav::string_parser parser(str);
        REQUIRE(parser.read_string_until("just") == "this is ");
        REQUIRE(parser.read_string_until("string") == " a random ");
    }

    SECTION("Test delimited string with include_delimiter") {
        const auto str = "this is just a random string";
        rav::string_parser parser(str);
        REQUIRE(parser.read_string_until("just", true) == "this is just");
        REQUIRE(parser.read_string_until("string", true) == " a random string");
    }

    SECTION("Test delimited string without include_delimiter") {
        const auto str = "key1=value1;key2=value2;key3=value3";
        rav::string_parser parser(str);
        REQUIRE(parser.read_string_until('=') == "key1");
        REQUIRE(parser.read_string_until(';') == "value1");
        REQUIRE(parser.read_string_until('=') == "key2");
        REQUIRE(parser.read_string_until(';') == "value2");
        REQUIRE(parser.read_string_until('=') == "key3");
        REQUIRE(parser.read_string_until(';') == "value3");
    }

    SECTION("Test delimited string with include_delimiter") {
        const auto str = "0.1.2.3";
        rav::string_parser parser(str);
        REQUIRE(parser.read_string_until('1', true) == "0.1");
        REQUIRE(parser.read_string_until('.', true) == ".");
        REQUIRE(parser.read_string_until('3', true) == "2.3");
    }

    SECTION("Test delimited string where delimiter is not found") {
        SECTION("Single char") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.read_string_until('4') == "0.1.2.3");
        }
        SECTION("Multi char") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.read_string_until("4") == "0.1.2.3");
        }
    }

    SECTION("Test string exhaustion") {
        SECTION("Single char") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.read_string_until('.') == "0");
            REQUIRE(parser.read_string_until('.') == "1");
            REQUIRE(parser.read_string_until('.') == "2");
            REQUIRE(parser.read_string_until('.') == "3");
            REQUIRE_FALSE(parser.read_string_until('.').has_value());
        }

        SECTION("Multiple chars") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.read_string_until(".") == "0");
            REQUIRE(parser.read_string_until(".") == "1");
            REQUIRE(parser.read_string_until(".") == "2");
            REQUIRE(parser.read_string_until(".") == "3");
            REQUIRE_FALSE(parser.read_string_until(".").has_value());
        }
    }

    SECTION("Parse some ints") {
        const auto str = "0.1.23456";
        rav::string_parser parser(str);
        REQUIRE(parser.read_int<int32_t>().value() == 0);
        REQUIRE_FALSE(parser.read_int<int32_t>().has_value());
        REQUIRE(parser.read_string_until('.')->empty());
        REQUIRE(parser.read_int<int32_t>().value() == 1);
        REQUIRE(parser.read_string_until('.')->empty());
        REQUIRE(parser.read_int<int32_t>().value() == 23456);
        REQUIRE_FALSE(parser.read_int<int32_t>().has_value());
    }

    SECTION("Parse some floats") {
        const auto str = "0.1.23456";
        rav::string_parser parser(str);
        REQUIRE(rav::util::is_within(parser.read_float().value(), 0.1f, 00001.f));
        REQUIRE(rav::util::is_within(parser.read_float().value(), 0.23456f, 00001.f));
        REQUIRE_FALSE(parser.read_float().has_value());
    }

    SECTION("Parse some doubles") {
        const auto str = "0.1.23456";
        rav::string_parser parser(str);
        REQUIRE(rav::util::is_within(parser.read_double().value(), 0.1, 00001.0));
        REQUIRE(rav::util::is_within(parser.read_double().value(), 0.23456, 00001.0));
        REQUIRE_FALSE(parser.read_float().has_value());
    }

    SECTION("Parse some LF lines") {
        const auto str = "line1\nline2\nline3";
        rav::string_parser parser(str);
        REQUIRE(parser.read_line() == "line1");
        REQUIRE(parser.read_line() == "line2");
        REQUIRE(parser.read_line() == "line3");
        REQUIRE_FALSE(parser.read_line().has_value());
    }

    SECTION("Parse some CRLF lines") {
        const auto str = "line1\r\nline2\r\nline3\r\n";
        rav::string_parser parser(str);
        REQUIRE(parser.read_line() == "line1");
        REQUIRE(parser.read_line() == "line2");
        REQUIRE(parser.read_line() == "line3");
        REQUIRE_FALSE(parser.read_line().has_value());
    }

    SECTION("Parse some mixed lines") {
        const auto str = "line1\r\nline2\nline3\r\n";
        rav::string_parser parser(str);
        REQUIRE(parser.read_line() == "line1");
        REQUIRE(parser.read_line() == "line2");
        REQUIRE(parser.read_line() == "line3");
        REQUIRE_FALSE(parser.read_line().has_value());
    }

    SECTION("Parse some empty lines") {
        const auto str = "line1\n\nline3\n";
        rav::string_parser parser(str);
        REQUIRE(parser.read_line() == "line1");
        REQUIRE(parser.read_line() == "");
        REQUIRE(parser.read_line() == "line3");
        REQUIRE_FALSE(parser.read_line().has_value());
    }

    SECTION("Skip character") {
        const auto str = "line";
        rav::string_parser parser(str);
        REQUIRE(parser.skip('l') == 1);
        REQUIRE(parser.read_line() == "ine");
    }

    SECTION("Skip character") {
        const auto str = "line";
        rav::string_parser parser(str);
        REQUIRE(parser.skip('a') == 0);
        REQUIRE(parser.read_line() == "line");
    }

    SECTION("Skip character") {
        const auto str = "";
        rav::string_parser parser(str);
        REQUIRE(parser.skip('a') == 0);
        REQUIRE_FALSE(parser.read_line().has_value());
    }

    SECTION("Skip characters") {
        const auto str = "line";
        rav::string_parser parser(str);
        REQUIRE(parser.skip("li") == 2);
        REQUIRE(parser.read_line() == "ne");
    }

    SECTION("Skip characters") {
        const auto str = "line";
        rav::string_parser parser(str);
        REQUIRE(parser.skip("aa") == 0);
        REQUIRE(parser.read_line() == "line");
    }

    SECTION("Skip characters") {
        const auto str = "";
        rav::string_parser parser(str);
        REQUIRE(parser.skip("aa") == 0);
        REQUIRE_FALSE(parser.read_line().has_value());
    }

    SECTION("Parse some refclk strings") {
        const auto str = "ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:5";
        rav::string_parser parser(str);
        REQUIRE(parser.read_string_until('=') == "ptp");
        REQUIRE(parser.read_string_until(':') == "IEEE1588-2008");
        REQUIRE(parser.read_string_until(':') == "39-A7-94-FF-FE-07-CB-D0");
        REQUIRE(parser.read_int<int32_t>().value() == 5);
    }
}
