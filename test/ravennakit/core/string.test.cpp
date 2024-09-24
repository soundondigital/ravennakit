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

TEST_CASE("string | up_to_first_occurrence_of", "[string]") {
    constexpr std::string_view haystack("one test two test three test");

    SECTION("Without substring included") {
        REQUIRE(rav::up_to_first_occurrence_of(haystack, "test", false) == "one ");
    }
    SECTION("With substring included") {
        REQUIRE(rav::up_to_first_occurrence_of(haystack, "test", true) == "one test");
    }
    SECTION("With invalid substring") {
        REQUIRE(rav::up_to_first_occurrence_of(haystack, "bleep", true).empty());
    }
}

TEST_CASE("string | up_to_the_nth_occurrence_of", "[string]") {
    std::string_view haystack("/one/two/three/four");

    SECTION("First occurrence") {
        REQUIRE(rav::up_to_the_nth_occurrence_of(1, haystack, "/", false).empty());
        REQUIRE(rav::up_to_the_nth_occurrence_of(1, haystack, "/", true) == "/");
    }

    SECTION("Second occurrence") {
        REQUIRE(rav::up_to_the_nth_occurrence_of(2, haystack, "/", false) == "/one");
        REQUIRE(rav::up_to_the_nth_occurrence_of(2, haystack, "/", true) == "/one/");
    }

    SECTION("Third occurrence") {
        REQUIRE(rav::up_to_the_nth_occurrence_of(3, haystack, "/", false) == "/one/two");
        REQUIRE(rav::up_to_the_nth_occurrence_of(3, haystack, "/", true) == "/one/two/");
    }

    SECTION("Fourth occurrence") {
        REQUIRE(rav::up_to_the_nth_occurrence_of(4, haystack, "/", false) == "/one/two/three");
        REQUIRE(rav::up_to_the_nth_occurrence_of(4, haystack, "/", true) == "/one/two/three/");
    }

    SECTION("Multi character needle") {
        REQUIRE(rav::up_to_the_nth_occurrence_of(4, haystack, "two", false).empty());
        REQUIRE(rav::up_to_the_nth_occurrence_of(4, haystack, "two", true).empty());
    }

    SECTION("Empty string") {
        REQUIRE(rav::up_to_the_nth_occurrence_of(1, "", "/", false).empty());
        REQUIRE(rav::up_to_the_nth_occurrence_of(1, "", "/", true).empty());
    }

    SECTION("Non-empty string with no needle") {
        REQUIRE(rav::up_to_the_nth_occurrence_of(1, "onetwothreefour", "/", false).empty());
        REQUIRE(rav::up_to_the_nth_occurrence_of(1, "onetwothreefour", "/", true).empty());
    }

    SECTION("Non-empty string with not enough needles needle") {
        REQUIRE(rav::up_to_the_nth_occurrence_of(2, "/onetwothreefour", "/", false).empty());
        REQUIRE(rav::up_to_the_nth_occurrence_of(2, "/onetwothreefour", "/", true).empty());
    }
}

TEST_CASE("string | up_to_last_occurrence_of", "[string]") {
    constexpr std::string_view haystack("one test two test three test");

    SECTION("Without substring included") {
        REQUIRE(rav::up_to_last_occurrence_of(haystack, "test", false) == "one test two test three ");
    }

    SECTION("With substring included") {
        REQUIRE(rav::up_to_last_occurrence_of(haystack, "test", true) == "one test two test three test");
    }

    SECTION("With invalid substring") {
        REQUIRE(rav::up_to_first_occurrence_of(haystack, "bleep", true).empty());
    }
}

TEST_CASE("string | from_first_occurrence_of", "[string]") {
    constexpr std::string_view haystack("one test two test three test");

    SECTION("Without substring included") {
        REQUIRE(rav::from_first_occurrence_of(haystack, "test", false) == " two test three test");
    }
    SECTION("With substring included") {
        REQUIRE(rav::from_first_occurrence_of(haystack, "test", true) == "test two test three test");
    }

    SECTION("With invalid substring") {
        REQUIRE(rav::from_first_occurrence_of(haystack, "bleep", true).empty());
    }
}

TEST_CASE("string | from_nth_occurrence_of", "[string]") {
    std::string_view haystack("/one/two/three/four");

    SECTION("First occurrence") {
        REQUIRE(rav::from_nth_occurrence_of(1, haystack, "/", false) == "one/two/three/four");
        REQUIRE(rav::from_nth_occurrence_of(1, haystack, "/", true) == "/one/two/three/four");
    }

    SECTION("Fourth occurrence") {
        REQUIRE(rav::from_nth_occurrence_of(4, haystack, "/", false) == "four");
        REQUIRE(rav::from_nth_occurrence_of(4, haystack, "/", true) == "/four");
    }

    SECTION("Multi character needle") {
        REQUIRE(rav::from_nth_occurrence_of(1, haystack, "two", false) == "/three/four");
        REQUIRE(rav::from_nth_occurrence_of(1, haystack, "two", true) == "two/three/four");
    }

    SECTION("Empty string") {
        REQUIRE(rav::from_nth_occurrence_of(1, "", "/", false).empty());
        REQUIRE(rav::from_nth_occurrence_of(1, "", "/", true).empty());
    }

    SECTION("Non-empty string with no needle") {
        REQUIRE(rav::from_nth_occurrence_of(1, "onetwothreefour", "/", false).empty());
        REQUIRE(rav::from_nth_occurrence_of(1, "onetwothreefour", "/", true).empty());
    }

    SECTION("Non-empty string with not enough needles needle") {
        REQUIRE(rav::from_nth_occurrence_of(2, "/onetwothreefour", "/", false).empty());
        REQUIRE(rav::from_nth_occurrence_of(2, "/onetwothreefour", "/", true).empty());
    }
}

TEST_CASE("string | from_last_occurrence_of", "[string]") {
    constexpr std::string_view haystack("one test two test three test");

    SECTION("Without substring included") {
        REQUIRE(rav::from_last_occurrence_of(haystack, "test", false).empty());
    }

    SECTION("With substring included") {
        REQUIRE(rav::from_last_occurrence_of(haystack, "test", true) == "test");
    }

    SECTION("With invalid substring") {
        REQUIRE(rav::from_first_occurrence_of(haystack, "bleep", true).empty());
    }
}

TEST_CASE("string | string_contains", "[string]") {
    SECTION("Without substring included") {
        REQUIRE(rav::string_contains("ABC", 'C'));
        REQUIRE_FALSE(rav::string_contains("ABC", 'c'));
        REQUIRE(rav::string_contains("ABC", 'B'));
    }
}

TEST_CASE("string | from_string_strict", "[string]") {
    SECTION("An integer should be successfully parsed") {
        auto result = rav::from_string_strict<int>("1");
        REQUIRE(result.has_value());
        REQUIRE(*result == 1);
    }

    SECTION("A string with non valid characters should not return a result") {
        auto result = rav::from_string_strict<int>("1 ");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A string with non valid characters should not return a result") {
        auto result = rav::from_string_strict<int>(" 1 ");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A string with non valid characters should not return a result") {
        auto result = rav::from_string_strict<int>(" 1");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A string with non valid characters should not return a result") {
        auto result = rav::from_string_strict<int>("1A");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A maximum value should be returned") {
        auto result = rav::from_string_strict<int>(std::to_string(std::numeric_limits<int>::max()));
        REQUIRE(result.has_value());
        REQUIRE(*result == std::numeric_limits<int>::max());
    }

    SECTION("A too big value should return no value") {
        auto result = rav::from_string_strict<int>(std::to_string(std::numeric_limits<int>::max()) + "1");
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("string | from_string", "[string]") {
    SECTION("An integer should be successfully parsed") {
        auto result = rav::from_string<int>("1");
        REQUIRE(result.has_value());
        REQUIRE(*result == 1);
    }

    SECTION("A string with non valid characters should still return a result") {
        auto result = rav::from_string<int>("1 ");
        REQUIRE(result.has_value());
        REQUIRE(*result == 1);
    }

    SECTION("A string with non valid characters should still return a result") {
        auto result = rav::from_string<int>("1A");
        REQUIRE(result.has_value());
        REQUIRE(*result == 1);
    }

    SECTION("A string starting with non valid characters should not return a result") {
        auto result = rav::from_string<int>(" 1 ");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A string starting with non valid characters should not return a result") {
        auto result = rav::from_string<int>(" 1");
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("A maximum value should be returned") {
        auto result = rav::from_string<int>(std::to_string(std::numeric_limits<int>::max()));
        REQUIRE(result.has_value());
        REQUIRE(*result == std::numeric_limits<int>::max());
    }

    SECTION("A too big value should return no value") {
        auto result = rav::from_string<int>(std::to_string(std::numeric_limits<int>::max()) + "1");
        REQUIRE_FALSE(result.has_value());
    }
}

TEST_CASE("string | remove_prefix", "[string]") {
    SECTION("Remove valid prefix") {
        std::string_view str = "some/random/string";
        REQUIRE(rav::remove_prefix(str, "some/"));
        REQUIRE(str == "random/string");
    }

    SECTION("Remove invalid prefix") {
        std::string_view str = "some/random/string";
        REQUIRE_FALSE(rav::remove_prefix(str, "noexist/"));
        REQUIRE(str == "some/random/string");
    }

    SECTION("Remove invalid prefix") {
        std::string_view str = "test/some/random/string";
        REQUIRE_FALSE(rav::remove_prefix(str, "some/"));
        REQUIRE(str == "test/some/random/string");
    }
}

TEST_CASE("string | remove_suffix", "[string]") {
    SECTION("Remove valid suffix") {
        std::string_view str = "some/random/string";
        REQUIRE(rav::remove_suffix(str, "/string"));
        REQUIRE(str == "some/random");
    }

    SECTION("Remove invalid suffix") {
        std::string_view str = "some/random/string";
        REQUIRE_FALSE(rav::remove_suffix(str, "/noexist"));
        REQUIRE(str == "some/random/string");
    }

    SECTION("Remove invalid suffix") {
        std::string_view str = "some/random/string/test";
        REQUIRE_FALSE(rav::remove_suffix(str, "/string"));
        REQUIRE(str == "some/random/string/test");
    }
}
