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

TEST_CASE("string_parser | split") {
    SECTION("Test delimited string without include_delimiter") {
        const auto str = "this is just a random string";
        rav::string_parser parser(str);
        REQUIRE(parser.split("just") == "this is ");
        REQUIRE(parser.split("string") == " a random ");
    }

    SECTION("Test delimited string with include_delimiter") {
        const auto str = "this is just a random string";
        rav::string_parser parser(str);
        REQUIRE(parser.split("just", true) == "this is just");
        REQUIRE(parser.split("string", true) == " a random string");
    }

    SECTION("Test delimited string without include_delimiter") {
        const auto str = "key1=value1;key2=value2;key3=value3";
        rav::string_parser parser(str);
        REQUIRE(parser.split('=') == "key1");
        REQUIRE(parser.split(';') == "value1");
        REQUIRE(parser.split('=') == "key2");
        REQUIRE(parser.split(';') == "value2");
        REQUIRE(parser.split('=') == "key3");
        REQUIRE(parser.split(';') == "value3");
    }

    SECTION("Test delimited string with include_delimiter") {
        const auto str = "0.1.2.3";
        rav::string_parser parser(str);
        REQUIRE(parser.split('1', true) == "0.1");
        REQUIRE(parser.split('.', true) == ".");
        REQUIRE(parser.split('3', true) == "2.3");
    }

    SECTION("Test delimited string with include_delimiter") {
        const auto str = "0.1.2.3";
        rav::string_parser parser(str);
        REQUIRE(parser.split('1') == "0.");
        REQUIRE(parser.split('.') == "");
        REQUIRE(parser.split('3') == "2.");
    }

    SECTION("Test delimited string where delimiter is not found") {
        SECTION("Single char") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.split('4') == "0.1.2.3");
        }
        SECTION("Multi char") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.split("4") == "0.1.2.3");
        }
    }

    SECTION("Test string exhaustion") {
        SECTION("Single char") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.split('.') == "0");
            REQUIRE(parser.split('.') == "1");
            REQUIRE(parser.split('.') == "2");
            REQUIRE(parser.split('.') == "3");
            REQUIRE_FALSE(parser.split('.').has_value());
        }

        SECTION("Multiple chars") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.split(".") == "0");
            REQUIRE(parser.split(".") == "1");
            REQUIRE(parser.split(".") == "2");
            REQUIRE(parser.split(".") == "3");
            REQUIRE_FALSE(parser.split(".").has_value());
        }
    }

    SECTION("Test string with only delimiter") {
        SECTION("Include delimiter") {
            const auto str = ".";
            rav::string_parser parser(str);
            REQUIRE(parser.split('.', true) == ".");
        }

        SECTION("Exclude delimiter") {
            const auto str = ".";
            rav::string_parser parser(str);
            REQUIRE(parser.split('.') == "");
        }
    }
}

TEST_CASE("string_parser | read_until") {
    SECTION("Test delimited string without include_delimiter") {
        const auto str = "this is just a random string";
        rav::string_parser parser(str);
        REQUIRE(parser.read_until("just") == "this is ");
        REQUIRE(parser.read_until("string") == " a random ");
    }

    SECTION("Test delimited string with include_delimiter") {
        const auto str = "this is just a random string";
        rav::string_parser parser(str);
        REQUIRE(parser.read_until("just", true) == "this is just");
        REQUIRE(parser.read_until("string", true) == " a random string");
    }

    SECTION("Test delimited string without include_delimiter") {
        const auto str = "key1=value1;key2=value2;key3=value3";
        rav::string_parser parser(str);
        REQUIRE(parser.read_until('=') == "key1");
        REQUIRE(parser.read_until(';') == "value1");
        REQUIRE(parser.read_until('=') == "key2");
        REQUIRE(parser.read_until(';') == "value2");
        REQUIRE(parser.read_until('=') == "key3");
        REQUIRE(parser.read_until(';') == std::nullopt);
    }

    SECTION("Test delimited string with include_delimiter") {
        const auto str = "0.1.2.3";
        rav::string_parser parser(str);
        REQUIRE(parser.read_until('1', true) == "0.1");
        REQUIRE(parser.read_until('.', true) == ".");
        REQUIRE(parser.read_until('3', true) == "2.3");
    }

    SECTION("Test delimited string with include_delimiter") {
        const auto str = "0.1.2.3";
        rav::string_parser parser(str);
        REQUIRE(parser.read_until('1') == "0.");
        REQUIRE(parser.read_until('.') == "");
        REQUIRE(parser.read_until('3') == "2.");
    }

    SECTION("Test delimited string where delimiter is not found") {
        SECTION("Single char") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.read_until('4') == std::nullopt);
        }
        SECTION("Multi char") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.read_until("4") == std::nullopt);
        }
    }

    SECTION("Test string exhaustion") {
        SECTION("Single char") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.read_until('.') == "0");
            REQUIRE(parser.read_until('.') == "1");
            REQUIRE(parser.read_until('.') == "2");
            REQUIRE(parser.read_until('.') == std::nullopt);
            REQUIRE_FALSE(parser.read_until('.').has_value());
        }

        SECTION("Multiple chars") {
            const auto str = "0.1.2.3";
            rav::string_parser parser(str);
            REQUIRE(parser.read_until(".") == "0");
            REQUIRE(parser.read_until(".") == "1");
            REQUIRE(parser.read_until(".") == "2");
            REQUIRE(parser.read_until(".") == std::nullopt);
            REQUIRE_FALSE(parser.read_until(".").has_value());
        }
    }

    SECTION("Test string with only delimiter") {
        SECTION("Include delimiter") {
            const auto str = ".";
            rav::string_parser parser(str);
            REQUIRE(parser.read_until('.', true) == ".");
        }

        SECTION("Exclude delimiter") {
            const auto str = ".";
            rav::string_parser parser(str);
            REQUIRE(parser.read_until('.') == "");
        }
    }
}

TEST_CASE("string_parser") {
    SECTION("Parse some ints") {
        const auto str = "0.1.23456";
        rav::string_parser parser(str);
        REQUIRE(parser.read_int<int32_t>().value() == 0);
        REQUIRE_FALSE(parser.read_int<int32_t>().has_value());
        REQUIRE(parser.split('.')->empty());
        REQUIRE(parser.read_int<int32_t>().value() == 1);
        REQUIRE(parser.split('.')->empty());
        REQUIRE(parser.read_int<int32_t>().value() == 23456);
        REQUIRE_FALSE(parser.read_int<int32_t>().has_value());
    }

    SECTION("Parse some ints") {
        const auto str = "11 22 33";
        rav::string_parser parser(str);
        REQUIRE(parser.read_int<int32_t>().value() == 11);
        REQUIRE(parser.read_int<int32_t>().value() == 22);
        REQUIRE(parser.read_int<int32_t>().value() == 33);
    }

    SECTION("Parse some floats") {
        const auto str = "0.1.23456";
        rav::string_parser parser(str);
        REQUIRE(rav::is_within(parser.read_float().value(), 0.1f, 00001.f));
        REQUIRE(rav::is_within(parser.read_float().value(), 0.23456f, 00001.f));
        REQUIRE_FALSE(parser.read_float().has_value());
    }

    SECTION("Parse some doubles") {
        const auto str = "0.1.23456";
        rav::string_parser parser(str);
        REQUIRE(rav::is_within(parser.read_double().value(), 0.1, 00001.0));
        REQUIRE(rav::is_within(parser.read_double().value(), 0.23456, 00001.0));
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

    SECTION("Parse string without newline") {
        const auto str = "line1";
        rav::string_parser parser(str);
        REQUIRE(parser.read_line() == "line1");
        REQUIRE_FALSE(parser.read_line().has_value());
    }

    SECTION("Parse empty string") {
        const auto str = "";
        rav::string_parser parser(str);
        REQUIRE_FALSE(parser.read_line().has_value());
    }

    SECTION("Read until newline") {
        const auto str = "line1\r\nline2\n\n\r\n";
        rav::string_parser parser(str);
        REQUIRE(parser.read_until_newline() == "line1");
        REQUIRE(parser.size() == strlen(str) - 7);
        REQUIRE(parser.read_until_newline() == "line2");
        REQUIRE(parser.size() == strlen(str) - 13);
        REQUIRE(parser.read_until_newline() == "");
        REQUIRE(parser.size() == strlen(str) - 14);
        REQUIRE(parser.read_until_newline() == "");
        REQUIRE(parser.size() == strlen(str) - 16);
        REQUIRE(parser.empty());
        REQUIRE(parser.exhausted());
    }

    SECTION("Read until newline on string without newline") {
        const auto str = "line1";
        rav::string_parser parser(str);
        REQUIRE_FALSE(parser.read_until_newline().has_value());
    }

    SECTION("Skip character") {
        const auto str = "line";
        rav::string_parser parser(str);
        REQUIRE(parser.skip('l'));
        REQUIRE(parser.read_line() == "ine");
    }

    SECTION("Skip character") {
        const auto str = "line";
        rav::string_parser parser(str);
        REQUIRE_FALSE(parser.skip('a'));
        REQUIRE(parser.read_line() == "line");
    }

    SECTION("Skip character") {
        const auto str = "";
        rav::string_parser parser(str);
        REQUIRE_FALSE(parser.skip('a'));
        REQUIRE_FALSE(parser.read_line().has_value());
    }

    SECTION("Skip characters") {
        const auto str = "line";
        rav::string_parser parser(str);
        REQUIRE(parser.skip("li"));
        REQUIRE(parser.read_line() == "ne");
    }

    SECTION("Skip characters") {
        const auto str = "line";
        rav::string_parser parser(str);
        REQUIRE_FALSE(parser.skip("aa"));
        REQUIRE(parser.read_line() == "line");
    }

    SECTION("Skip characters") {
        const auto str = "";
        rav::string_parser parser(str);
        REQUIRE_FALSE(parser.skip("aa"));
        REQUIRE_FALSE(parser.read_line().has_value());
    }

    SECTION("Skip n characters") {
        const auto str = "++++++++a";
        rav::string_parser parser(str);
        REQUIRE(parser.skip_n('+', 4) == 4);
        REQUIRE(parser.read_line() == "++++a");
    }

    SECTION("Skip n characters") {
        const auto str = "++++a";
        rav::string_parser parser(str);
        REQUIRE(parser.skip_n('+', 4) == 4);
        REQUIRE(parser.read_line() == "a");
    }

    SECTION("Skip n characters") {
        const auto str = "+++a";
        rav::string_parser parser(str);
        REQUIRE(parser.skip_n('+', 4) == 3);
        REQUIRE(parser.read_line() == "a");
    }

    SECTION("Skip n characters") {
        const auto str = "a";
        rav::string_parser parser(str);
        REQUIRE(parser.skip_n('+', 4) == 0);
        REQUIRE(parser.read_line() == "a");
    }

    SECTION("Skip n characters") {
        const auto str = "";
        rav::string_parser parser(str);
        REQUIRE(parser.skip_n('+', 4) == 0);
    }

    SECTION("Parse some refclk strings") {
        const auto str = "ptp=IEEE1588-2008:39-A7-94-FF-FE-07-CB-D0:5";
        rav::string_parser parser(str);
        REQUIRE(parser.split('=') == "ptp");
        REQUIRE(parser.split(':') == "IEEE1588-2008");
        REQUIRE(parser.split(':') == "39-A7-94-FF-FE-07-CB-D0");
        REQUIRE(parser.read_int<int32_t>().value() == 5);
    }
}
