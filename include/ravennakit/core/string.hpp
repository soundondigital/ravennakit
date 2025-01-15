/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#pragma once

#include <algorithm>
#include <charconv>
#include <optional>
#include <sstream>
#include <string_view>
#include <vector>

namespace rav {

/**
 * Returns a string truncated up to the first occurrence of a needle.
 * @param string_to_search_in String to search in.
 * @param string_to_search_for String to search for.
 * @param include_sub_string_in_result If true the needle will be included in the resulting string, when false it will
 * not.
 * @return The truncated string, or an empty string when no needle was found.
 */
inline std::string_view up_to_first_occurrence_of(
    std::string_view string_to_search_in, const std::string_view string_to_search_for,
    const bool include_sub_string_in_result
) {
    const auto pos = string_to_search_in.find(string_to_search_for);

    if (pos == std::string_view::npos)
        return {};

    return string_to_search_in.substr(0, include_sub_string_in_result ? pos + string_to_search_for.size() : pos);
}

/**
 * Returns a string truncated up to the nth occurrence of a needle.
 * @param nth Nth needle to find, 0 will always return an empty string, 1 will find the 1st needle, and so on.
 * @param string_to_search_in String to search in.
 * @param string_to_search_for String to search for.
 * @param include_sub_string_in_result If true the needle will be included in the resulting string, when false it will
 * not.
 * @return The truncated string, or an empty string when no needle was found.
 */
inline std::string_view up_to_the_nth_occurrence_of(
    const size_t nth, std::string_view string_to_search_in, const std::string_view string_to_search_for,
    const bool include_sub_string_in_result
) {
    size_t pos = std::string_view::npos;

    for (size_t i = 0; i < nth; ++i) {
        pos = string_to_search_in.find(string_to_search_for, pos + 1);

        if (pos == std::string_view::npos) {
            return {};
        }
    }

    if (pos == std::string_view::npos) {
        return {};
    }

    return string_to_search_in.substr(0, include_sub_string_in_result ? pos + string_to_search_for.size() : pos);
}

/**
 * Returns a string truncated up to the last occurrence of a needle.
 * @param string_to_search_in String to search in.
 * @param string_to_search_for String to search for.
 * @param include_sub_string_in_result If true the needle will be included in the resulting string, when false it will
 * not.
 * @return The truncated string, or an empty string when no needle was found.
 */
inline std::string_view up_to_last_occurrence_of(
    const std::string_view string_to_search_in, const std::string_view string_to_search_for,
    const bool include_sub_string_in_result
) {
    const auto pos = string_to_search_in.rfind(string_to_search_for);

    if (pos == std::string_view::npos)
        return {};

    return string_to_search_in.substr(0, include_sub_string_in_result ? pos + string_to_search_for.size() : pos);
}

/**
 * Returns a string truncated starting from to first last occurrence of a needle.
 * @param string_to_search_in String to search in.
 * @param string_to_search_for String to search for.
 * @param include_sub_string_in_result If true the needle will be included in the resulting string, when false it will
 * not.
 * @return The truncated string, or an empty string when no needle was found.
 */
inline std::string_view from_first_occurrence_of(
    std::string_view string_to_search_in, const std::string_view string_to_search_for,
    const bool include_sub_string_in_result
) {
    const auto pos = string_to_search_in.find(string_to_search_for);

    if (pos == std::string_view::npos)
        return {};

    return string_to_search_in.substr(include_sub_string_in_result ? pos : pos + string_to_search_for.size());
}

/**
 * Returns a string truncated from the nth occurrence of a needle.
 * @param nth Nth needle to find, 0 will always return the full string, 1 will find the 1st needle, and so on.
 * @param string_to_search_in String to search in.
 * @param string_to_search_for String to search for.
 * @param include_sub_string_in_result If true the needle will be included in the resulting string, when false it will
 * not.
 * @return The truncated string, or an empty string when no needle was found.
 */
inline std::string_view from_nth_occurrence_of(
    const size_t nth, std::string_view string_to_search_in, const std::string_view string_to_search_for,
    const bool include_sub_string_in_result
) {
    size_t pos = std::string_view::npos;

    for (size_t i = 0; i < nth; ++i) {
        pos = string_to_search_in.find(string_to_search_for, pos + 1);

        if (pos == std::string_view::npos) {
            return {};
        }
    }

    if (pos == std::string_view::npos) {
        return {};
    }

    return string_to_search_in.substr(
        include_sub_string_in_result ? pos : pos + string_to_search_for.size(), string_to_search_in.size()
    );
}

/**
 * Returns a string truncated starting from to last occurrence of a needle.
 * @param string_to_search_in String to search in.
 * @param string_to_search_for String to search for.
 * @param include_sub_string_in_result If true the needle will be included in the resulting string, when false it will
 * not.
 * @return The truncated string, or an empty string when no needle was found.
 */
inline std::string_view from_last_occurrence_of(
    const std::string_view string_to_search_in, const std::string_view string_to_search_for,
    const bool include_sub_string_in_result
) {
    const auto pos = string_to_search_in.rfind(string_to_search_for);

    if (pos == std::string_view::npos) {
        return {};
    }

    return string_to_search_in.substr(include_sub_string_in_result ? pos : pos + string_to_search_for.size());
}

/**
 * Removes given prefix from given string_view, if prefix is found at the beginning of string.
 * @param string String to remove prefix from.
 * @param prefix_to_remove Prefix to find and remove.
 * @return True if prefix was removed, or false if prefix was not found and string was not altered.
 */
inline bool remove_prefix(std::string_view& string, const std::string_view prefix_to_remove) {
    const auto pos = string.find(prefix_to_remove);

    if (pos != 0)  // If prefix doesn't start at the beginning.
        return false;

    string = string.substr(pos + prefix_to_remove.size());
    return true;
}

/**
 * Removes given suffix from given string_view, if suffix is found at the end of string.
 * @param string String to remove suffix from.
 * @param suffix_to_remove Suffix to find and remove.
 * @return True if suffix was removed, or false if suffix was not found and string was not altered.
 */
inline bool remove_suffix(std::string_view& string, const std::string_view suffix_to_remove) {
    const auto pos = string.rfind(suffix_to_remove);

    if (pos != string.size() - suffix_to_remove.size()) {
        return false;
    }

    string = string.substr(0, pos);

    return true;
}

/**
 * String to number - a small convenience function around std::from_chars.
 * @tparam Type Type of the value to convert from a string.
 * @param string String to convert to a value.
 * @param strict If true, the whole string must be a number, otherwise only the beginning of the string must be a
 * number.
 * @param base Base of the number to convert. When 16, the "0x" and "0X" prefixes are not recognized.
 * @return The converted value as optional, which will contain a value on success or will be empty on failure.
 */
template<typename Type>
std::optional<Type> ston(std::string_view string, const bool strict = false, const int base = 10) {
    Type result {};
    auto [p, ec] = std::from_chars(string.data(), string.data() + string.size(), result, base);
    if (ec == std::errc() && (!strict || p >= string.data() + string.size()))
        return result;
    return {};
}

/**
 * String to double - converts a string to a double, or returns an empty optional if the conversion failed.
 * @param str String to convert.
 * @return The converted double, or an empty optional if the conversion failed.
 */
inline std::optional<double> stod(const std::string& str) {
    try {
        return std::stod(str);
    } catch (...) {
        return std::nullopt;
    }
}

/**
 * Tests whether given text starts with a certain string.
 * @param text The text to test.
 * @param starts_with The string to test for.
 * @return True if text starts with starts_with, false otherwise.
 */
inline bool string_starts_with(const std::string_view text, const std::string_view starts_with) {
    return text.rfind(starts_with, 0) == 0;
}

/**
 * Returns whether given string contains a certain character.
 * @param string String to look into.
 * @param c Character to find.
 * @return True if character was found, of false if character was not found.
 */
inline bool string_contains(const std::string_view string, char c) {
    return std::any_of(string.begin(), string.end(), [c](const char& item) {
        return item == c;
    });
}

/**
 * Splits a string into a vector of strings based on a delimiter.
 * @param string The string to split.
 * @param delimiter The delimiter to split the string by.
 * @return A vector of strings.
 */
inline std::vector<std::string> string_split(const std::string& string, const char* delimiter) {
    std::vector<std::string> results;

    size_t prev = 0;
    size_t next = 0;

    while ((next = string.find_first_of(delimiter, prev)) != std::string::npos) {
        if (next - prev != 0) {
            results.push_back(string.substr(prev, next - prev));
        }
        prev = next + 1;
    }

    if (prev < string.size()) {
        results.push_back(string.substr(prev));
    }

    return results;
}

/**
 * Splits a string into a vector of strings based on a delimiter.
 * @param string The string to split.
 * @param delimiter The delimiter to split the string by.
 * @return A vector of strings.
 */
inline std::vector<std::string> string_split(const std::string& string, const char delimiter) {
    const char delimiter_string[2] = {delimiter, '\0'};
    return string_split(string, delimiter_string);
}

/**
 * Replaces all sequences of to_replace with replacement.
 * @param original The original string,
 * @param to_replace The sequence to replace.
 * @param replacement The replacement.
 * @return Modified string, or original string if sequence was not found.
 */
inline std::string
string_replace(const std::string& original, const std::string& to_replace, const std::string& replacement) {
    std::string modified = original;
    size_t pos = 0;

    while ((pos = modified.find(to_replace, pos)) != std::string::npos) {
        modified.replace(pos, to_replace.length(), replacement);
        pos += replacement.length();  // Move past the replacement to avoid infinite loop
    }

    return modified;
}

/**
 * Compares 2 strings case-insensitively.
 * @param lhs Left hand side
 * @param rhs Right hand side
 * @return True if strings are equal, false otherwise.
 */
inline bool string_compare_case_insensitive(const std::string_view lhs, const std::string_view rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }

    for (size_t i = 0; i < lhs.size(); ++i) {
        if (std::tolower(lhs[i]) != std::tolower(rhs[i])) {
            return false;
        }
    }

    return true;
}

}  // namespace rav
