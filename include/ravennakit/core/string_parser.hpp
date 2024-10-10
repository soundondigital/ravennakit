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

#include <charconv>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>

#include "assert.hpp"
#include "constants.hpp"
#include "string.hpp"

namespace rav {

/**
 * A handy utility class for parsing strings. It works like a stream, where it maintains a position in the string and
 * subsequent calls will read from that position.
 */
class string_parser {
  public:
    /**
     * Constructs a parser from given string view. Doesn't take ownership of the string, so make sure for the original
     * string to outlive this parser instance.
     * @param str The string to parse.
     */
    explicit string_parser(const std::string_view str) : str_(str) {}

    /**
     * Constructs a parser from given c-string and size. Doesn't take ownership of the string, so make sure for the
     * original string to outlive this parser instance.
     * @param str The string to parse.
     * @param size The size of the string (without terminating null character).
     */
    string_parser(const char* str, const size_t size) : str_({str, size}) {}

    /**
     * Constructs a parser from given c-string. String must be null-terminated. Doesn't take ownership of the string, so
     * make sure for the original string to outlive this parser instance.
     * @param str The string to parse.
     */
    explicit string_parser(const char* str) : str_ {str, std::strlen(str)} {}

    /**
     * Constructs a parser from given string. Doesn't take ownership of the string, so make sure for the original string
     * to outlive this parser instance.
     * @param str The string to parse.
     */
    explicit string_parser(const std::string& str) : str_(str) {}

    /**
     * Reads a string until the given delimiter. If delimiter is not found, the whole string is returned.
     * @param delimiter The character sequence to read until.
     * @param include_delimiter Whether to include the delimiter in the returned string.
     * @return The read string, or an empty optional if the string is exhausted.
     */
    std::optional<std::string_view> read_until(const char delimiter, const bool include_delimiter = false) {
        if (str_.empty()) {
            return std::nullopt;
        }

        const auto pos = str_.find(delimiter);
        if (pos == std::string_view::npos) {
            auto str = str_;
            str_ = {};
            return str;
        }

        const auto substr = str_.substr(0, include_delimiter ? pos + 1 : pos);
        str_.remove_prefix(pos + 1);
        return substr;  // NOLINT: The address of the local variable 'substr' may escape the function
    }

    /**
     * Reads the rest of the string.
     * @return The read string.
     */
    std::optional<std::string_view> read_until_end() {
        if (str_.empty()) {
            return std::nullopt;
        }
        auto str = str_;
        str_ = {};
        return str;
    }

    /**
     * Reads a string until the given delimiter. If delimiter is not found, the whole string is returned.
     * @param delimiter The character sequence to read until.
     * @param include_delimiter Whether to include the delimiter in the returned string.
     * @return The read string.
     */
    std::optional<std::string_view> read_until(const char* delimiter, const bool include_delimiter = false) {
        if (str_.empty()) {
            return std::nullopt;
        }

        const auto pos = str_.find(delimiter);
        if (pos == std::string_view::npos) {
            auto str = str_;
            str_ = {};
            return str;
        }

        const auto substr = str_.substr(0, include_delimiter ? pos + strlen(delimiter) : pos);
        str_.remove_prefix(pos + strlen(delimiter));
        return substr;  // NOLINT: The address of the local variable 'substr' may escape the function
    }

    /**
     * Reads a line from the string. A line is considered to be terminated by a newline character or by the end of the
     * string.
     * @returns The read line.
     */
    std::optional<std::string_view> read_line() {
        if (str_.empty()) {
            return std::nullopt;
        }

        const auto pos = str_.find('\n');
        if (pos == std::string_view::npos) {
            const auto str = str_;
            str_ = {};
            return str;  // NOLINT: The address of the local variable 'substr' may escape the function
        }

        auto substr = str_.substr(0, pos);
        str_.remove_prefix(pos + 1);
        if (!substr.empty() && substr.back() == '\r') {
            substr.remove_suffix(1);  // Remove CR from CRLF
        }
        return substr;  // NOLINT: The address of the local variable 'substr' may escape the function
    }

    /**
     * Tries to read an integer from the string. If successful, the integer is returned, otherwise an empty optional is
     * returned. If the string starts with spaces, they are skipped before parsing the number.
     * @tparam T The type of the integer to read.
     * @return The read integer or an empty optional.
     */
    template<class T>
    std::optional<T> read_int() {
        T value;
        if (skip_n(' ', RAV_LOOP_UPPER_BOUND) == RAV_LOOP_UPPER_BOUND) {
            RAV_ASSERT_FALSE("Loop upper bound reached while skipping spaces");
        }
        const auto result = std::from_chars(str_.data(), str_.data() + str_.size(), value);
        if (result.ec == std::errc()) {
            str_.remove_prefix(static_cast<size_t>(result.ptr - str_.data()));
            return value;
        }
        return std::nullopt;
    }

    /**
     * Tries to read a float from the string. If successful, the float is returned, otherwise an empty optional is
     * returned.
     * @return The read float or an empty optional.
     */
    std::optional<float> read_float() {
        // Sadly this might allocate some memory, but I haven't found another way of reading a double from a
        // string_view. I have considered using std::strtof, but it requires a null-terminated string. Also, in newer
        // versions of C++ support from_chars for floats and doubles, but at this point in time I'm stuck with c++17.
        // Luckily, the performance impact should be minimal, especially SSO will be engaged in most cases.
        const std::string str(str_.data(), str_.size());
        size_t pos = 0;
        try {
            auto value = std::stof(str, &pos);
            str_.remove_prefix(pos);
            return value;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * Tries to read a float from the string. If successful, the float is returned, otherwise an empty optional is
     * returned.
     * @return The read float or an empty optional.
     */
    std::optional<double> read_double() {
        // Sadly this might allocate some memory, but I haven't found another way of reading a double from a
        // string_view. I have considered using std::strtof, but it requires a null-terminated string. Also, in newer
        // versions of C++ support from_chars for floats and doubles, but at this point in time I'm stuck with c++17.
        // Luckily, the performance impact should be minimal, especially SSO will be engaged in most cases.
        const std::string str(str_.data(), str_.size());
        size_t pos = 0;
        try {
            auto value = std::stod(str, &pos);
            str_.remove_prefix(pos);
            return value;
        } catch (...) {
            return std::nullopt;
        }
    }

    /**
     * Skips the given sequence of characters from the beginning of the string.
     * @param chars The characters to skip.
     * @return True if the sequence was skipped, or false otherwise.
     */
    bool skip(const char* chars) {
        if (str_.rfind(chars, 0) == 0) {
            str_.remove_prefix(std::strlen(chars));
            return true;
        }

        return false;
    }

    /**
     * Skips the given character from the beginning of the string.
     * @param chr The character to skip.
     * @return True if the sequence was skipped, or false otherwise.
     */
    bool skip(const char chr) {
        if (str_.rfind(chr, 0) == 0) {
            str_.remove_prefix(1);
            return true;
        }

        return false;
    }

    /**
     * Skips n amount of characters from the beginning of the string.
     * @param chr The character to skip.
     * @param count The number of characters to skip.
     * @return The number of characters skipped.
     */
    uint64_t skip_n(const char chr, const size_t count) {
        uint64_t i = 0;
        for (; i < std::min(count, str_.size()); ++i) {
            if (str_[i] != chr) {
                break;
            }
        }
        str_.remove_prefix(i);
        return i;
    }

    /**
     * @return True if the string is exhausted, or false otherwise.
     */
    [[nodiscard]] bool exhausted() const {
        return str_.empty();
    }

  private:
    std::string_view str_;
    // size_t pos_ {0};
};

}  // namespace rav
