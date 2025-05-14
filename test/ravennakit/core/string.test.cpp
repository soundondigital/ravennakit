/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/string.hpp"

#include <catch2/catch_all.hpp>

#include "ravennakit/core/util.hpp"

TEST_CASE("string | up_to_first_occurrence_of") {
    constexpr std::string_view haystack("one test two test three test");

    SECTION("Without substring included") {
        REQUIRE(rav::string_up_to_first_occurrence_of(haystack, "test", false) == "one ");
    }
    SECTION("With substring included") {
        REQUIRE(rav::string_up_to_first_occurrence_of(haystack, "test", true) == "one test");
    }
    SECTION("With invalid substring") {
        REQUIRE(rav::string_up_to_first_occurrence_of(haystack, "bleep", true).empty());
    }
}

TEST_CASE("string | up_to_the_nth_occurrence_of") {
    std::string_view haystack("/one/two/three/four");

    SECTION("First occurrence") {
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(1, haystack, "/", false).empty());
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(1, haystack, "/", true) == "/");
    }

    SECTION("Second occurrence") {
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(2, haystack, "/", false) == "/one");
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(2, haystack, "/", true) == "/one/");
    }

    SECTION("Third occurrence") {
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(3, haystack, "/", false) == "/one/two");
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(3, haystack, "/", true) == "/one/two/");
    }

    SECTION("Fourth occurrence") {
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(4, haystack, "/", false) == "/one/two/three");
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(4, haystack, "/", true) == "/one/two/three/");
    }

    SECTION("Multi character needle") {
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(4, haystack, "two", false).empty());
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(4, haystack, "two", true).empty());
    }

    SECTION("Empty string") {
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(1, "", "/", false).empty());
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(1, "", "/", true).empty());
    }

    SECTION("Non-empty string with no needle") {
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(1, "onetwothreefour", "/", false).empty());
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(1, "onetwothreefour", "/", true).empty());
    }

    SECTION("Non-empty string with not enough needles needle") {
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(2, "/onetwothreefour", "/", false).empty());
        REQUIRE(rav::string_up_to_the_nth_occurrence_of(2, "/onetwothreefour", "/", true).empty());
    }
}

TEST_CASE("string | up_to_last_occurrence_of") {
    constexpr std::string_view haystack("one test two test three test");

    SECTION("Without substring included") {
        REQUIRE(rav::string_up_to_last_occurrence_of(haystack, "test", false) == "one test two test three ");
    }

    SECTION("With substring included") {
        REQUIRE(rav::string_up_to_last_occurrence_of(haystack, "test", true) == "one test two test three test");
    }

    SECTION("With invalid substring") {
        REQUIRE(rav::string_up_to_first_occurrence_of(haystack, "bleep", true).empty());
    }
}

TEST_CASE("string | from_first_occurrence_of") {
    constexpr std::string_view haystack("one test two test three test");

    SECTION("Without substring included") {
        REQUIRE(rav::string_from_first_occurrence_of(haystack, "test", false) == " two test three test");
    }
    SECTION("With substring included") {
        REQUIRE(rav::string_from_first_occurrence_of(haystack, "test", true) == "test two test three test");
    }

    SECTION("With invalid substring") {
        REQUIRE(rav::string_from_first_occurrence_of(haystack, "bleep", true).empty());
    }
}

TEST_CASE("string | from_nth_occurrence_of") {
    std::string_view haystack("/one/two/three/four");

    SECTION("First occurrence") {
        REQUIRE(rav::string_from_nth_occurrence_of(1, haystack, "/", false) == "one/two/three/four");
        REQUIRE(rav::string_from_nth_occurrence_of(1, haystack, "/", true) == "/one/two/three/four");
    }

    SECTION("Fourth occurrence") {
        REQUIRE(rav::string_from_nth_occurrence_of(4, haystack, "/", false) == "four");
        REQUIRE(rav::string_from_nth_occurrence_of(4, haystack, "/", true) == "/four");
    }

    SECTION("Multi character needle") {
        REQUIRE(rav::string_from_nth_occurrence_of(1, haystack, "two", false) == "/three/four");
        REQUIRE(rav::string_from_nth_occurrence_of(1, haystack, "two", true) == "two/three/four");
    }

    SECTION("Empty string") {
        REQUIRE(rav::string_from_nth_occurrence_of(1, "", "/", false).empty());
        REQUIRE(rav::string_from_nth_occurrence_of(1, "", "/", true).empty());
    }

    SECTION("Non-empty string with no needle") {
        REQUIRE(rav::string_from_nth_occurrence_of(1, "onetwothreefour", "/", false).empty());
        REQUIRE(rav::string_from_nth_occurrence_of(1, "onetwothreefour", "/", true).empty());
    }

    SECTION("Non-empty string with not enough needles needle") {
        REQUIRE(rav::string_from_nth_occurrence_of(2, "/onetwothreefour", "/", false).empty());
        REQUIRE(rav::string_from_nth_occurrence_of(2, "/onetwothreefour", "/", true).empty());
    }
}

TEST_CASE("string | from_last_occurrence_of") {
    constexpr std::string_view haystack("one test two test three test");

    SECTION("Without substring included") {
        REQUIRE(rav::string_from_last_occurrence_of(haystack, "test", false).empty());
    }

    SECTION("With substring included") {
        REQUIRE(rav::string_from_last_occurrence_of(haystack, "test", true) == "test");
    }

    SECTION("With invalid substring") {
        REQUIRE(rav::string_from_first_occurrence_of(haystack, "bleep", true).empty());
    }
}

TEST_CASE("string | string_contains") {
    SECTION("Without substring included") {
        REQUIRE(rav::string_contains("ABC", 'C'));
        REQUIRE_FALSE(rav::string_contains("ABC", 'c'));
        REQUIRE(rav::string_contains("ABC", 'B'));
    }
}

TEST_CASE("string | from_string_strict") {
    SECTION("An integer should be successfully parsed") {
        auto result = rav::string_to_int<int>("1", true);
        REQUIRE(result.has_value());
        REQUIRE(*result == 1);
    }

    SECTION("A string with non valid characters should not return a result") {
        auto result = rav::string_to_int<int>("1 ", true);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A string with non valid characters should not return a result") {
        auto result = rav::string_to_int<int>(" 1 ", true);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A string with non valid characters should not return a result") {
        auto result = rav::string_to_int<int>(" 1", true);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A string with non valid characters should not return a result") {
        auto result = rav::string_to_int<int>("1A", true);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A maximum value should be returned") {
        auto result = rav::string_to_int<int>(std::to_string(std::numeric_limits<int>::max()), true);
        REQUIRE(result.has_value());
        REQUIRE(*result == std::numeric_limits<int>::max());
    }

    SECTION("A too big value should return no value") {
        auto result = rav::string_to_int<int>(std::to_string(std::numeric_limits<int>::max()) + "1", true);
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("string | from_string") {
    SECTION("An integer should be successfully parsed") {
        auto result = rav::string_to_int<int>("1");
        REQUIRE(result.has_value());
        REQUIRE(*result == 1);
    }

    SECTION("A string with non valid characters should still return a result") {
        auto result = rav::string_to_int<int>("1 ");
        REQUIRE(result.has_value());
        REQUIRE(*result == 1);
    }

    SECTION("A string with non valid characters should still return a result") {
        auto result = rav::string_to_int<int>("1A");
        REQUIRE(result.has_value());
        REQUIRE(*result == 1);
    }

    SECTION("A string starting with non valid characters should not return a result") {
        auto result = rav::string_to_int<int>(" 1 ");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A string starting with non valid characters should not return a result") {
        auto result = rav::string_to_int<int>(" 1");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A maximum value should be returned") {
        auto result = rav::string_to_int<int>(std::to_string(std::numeric_limits<int>::max()));
        REQUIRE(result.has_value());
        REQUIRE(*result == std::numeric_limits<int>::max());
    }

    SECTION("A too big value should return no value") {
        auto result = rav::string_to_int<int>(std::to_string(std::numeric_limits<int>::max()) + "1");
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("string | remove_prefix") {
    SECTION("Remove valid prefix") {
        REQUIRE(rav::string_remove_prefix("some/random/string", "some/") == "random/string");
    }

    SECTION("Remove invalid prefix") {
        REQUIRE(rav::string_remove_prefix("some/random/string", "noexist/") == "some/random/string");
    }

    SECTION("Remove invalid prefix") {
        REQUIRE(rav::string_remove_prefix("test/some/random/string", "some/") == "test/some/random/string");
    }
}

TEST_CASE("string | remove_suffix") {
    SECTION("Remove valid suffix") {
        REQUIRE(rav::string_remove_suffix("some/random/string", "/string") == "some/random");
    }

    SECTION("Remove invalid suffix") {
        REQUIRE(rav::string_remove_suffix("some/random/string", "/noexist") == "some/random/string");
    }

    SECTION("Remove invalid suffix") {
        REQUIRE(rav::string_remove_suffix("some/random/string/test", "/string") == "some/random/string/test");
    }
}

TEST_CASE("string | split_string") {
    SECTION("Test string with char delimiter") {
        const std::string text = "line1 line2 line3";
        auto result = rav::string_split(text, ' ');
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == "line1");
        REQUIRE(result[1] == "line2");
        REQUIRE(result[2] == "line3");
    }

    SECTION("Extra spaces should not create empty strings") {
        const std::string text = " line1 line2  line3";
        auto result = rav::string_split(text, ' ');
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == "line1");
        REQUIRE(result[1] == "line2");
        REQUIRE(result[2] == "line3");
    }

    SECTION("Test string with char delimiter") {
        const std::string text = "__line1__line2__line3__";
        auto result = rav::string_split(text, "__");
        REQUIRE(result.size() == 3);
        REQUIRE(result[0] == "line1");
        REQUIRE(result[1] == "line2");
        REQUIRE(result[2] == "line3");
    }

    SECTION("Test string without delimiter") {
        const std::string text = "line1";
        auto result = rav::string_split(text, ' ');
        REQUIRE(result.size() == 1);
        REQUIRE(result[0] == "line1");
    }
}

TEST_CASE("string | stod") {
    SECTION("1.0") {
        const auto v = rav::string_to_double("1.0");
        REQUIRE(v.has_value());
    }

    SECTION(" 1.0") {
        const auto v = rav::string_to_double(" 1.0");
        REQUIRE(v.has_value());
    }

    SECTION(" 1.0abc") {
        const auto v = rav::string_to_double(" 1.0abc");
        REQUIRE(v.has_value());
    }

    SECTION(" 1.0 abc") {
        const auto v = rav::string_to_double(" 1.0 abc");
        REQUIRE(v.has_value());
    }

    SECTION("Some error cases") {
        REQUIRE_FALSE(rav::string_to_double("").has_value());
        REQUIRE_FALSE(rav::string_to_double("abc").has_value());
        REQUIRE_FALSE(rav::string_to_double(" abc 1.0").has_value());
        REQUIRE_FALSE(rav::string_to_double(" abc1.0").has_value());
    }
}

TEST_CASE("string | string_replace") {
    SECTION("Replace single char") {
        std::string original("abcdef");
        REQUIRE(rav::string_replace(original, "c", "!") == "ab!def");
    }

    SECTION("Replace sequence") {
        std::string original("abcdef");
        REQUIRE(rav::string_replace(original, "abc", "12") == "12def");
    }

    SECTION("Replace invalid") {
        std::string original("abcdef");
        REQUIRE(rav::string_replace(original, "ghi", "12") == "abcdef");
    }

    SECTION("Replace multiple sequences") {
        std::string original("abcdefabcdef");
        REQUIRE(rav::string_replace(original, "abc", "12") == "12def12def");
    }

    SECTION("Replace multiple sequences") {
        std::string original("abcdef\r\n");
        REQUIRE(rav::string_replace(original, "\r\n", "\n") == "abcdef\n");
    }
}

TEST_CASE("string | string_compare_case_insensitive") {
    SECTION("Equal strings") {
        REQUIRE(rav::string_compare_case_insensitive("abc", "abc"));
    }

    SECTION("Equal strings with different case") {
        REQUIRE(rav::string_compare_case_insensitive("abc", "ABC"));
    }

    SECTION("Different strings") {
        REQUIRE_FALSE(rav::string_compare_case_insensitive("abc", "def"));
    }

    SECTION("Different strings with different case") {
        REQUIRE_FALSE(rav::string_compare_case_insensitive("abc", "DEF"));
    }

    SECTION("Strings with different length") {
        REQUIRE_FALSE(rav::string_compare_case_insensitive("abc", "abcd"));
    }
}

TEST_CASE("string | string_to_upper") {
    SECTION("Lower") {
        const std::string str("abc");
        REQUIRE(rav::string_to_upper(str) == "ABC");
        REQUIRE(rav::string_to_upper(str, 1) == "Abc");
        REQUIRE(rav::string_to_upper(str, 2) == "ABc");
        REQUIRE(rav::string_to_upper(str, 3) == "ABC");
    }

    SECTION("Upper") {
        const std::string str("ABC");
        REQUIRE(rav::string_to_upper(str) == "ABC");
        REQUIRE(rav::string_to_upper(str, 1) == "ABC");
        REQUIRE(rav::string_to_upper(str, 2) == "ABC");
        REQUIRE(rav::string_to_upper(str, 3) == "ABC");
    }
}

TEST_CASE("string | string_to_lower") {
    SECTION("Upper") {
        const std::string str("ABC");
        REQUIRE(rav::string_to_lower(str) == "abc");
        REQUIRE(rav::string_to_lower(str, 1) == "aBC");
        REQUIRE(rav::string_to_lower(str, 2) == "abC");
        REQUIRE(rav::string_to_lower(str, 3) == "abc");
    }

    SECTION("Lower") {
        const std::string str("abc");
        REQUIRE(rav::string_to_lower(str) == "abc");
        REQUIRE(rav::string_to_lower(str, 1) == "abc");
        REQUIRE(rav::string_to_lower(str, 2) == "abc");
        REQUIRE(rav::string_to_lower(str, 3) == "abc");
    }
}
