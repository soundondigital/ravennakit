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
#include "ravennakit/core/assert.hpp"

#include <tuple>

namespace rav {

/**
 * Defines the base logic for parsing RTSP requests and responses. This class is meant to be inherited by specific
 * request and response parsers.
 */
class rtsp_parser_base {
  public:
    virtual ~rtsp_parser_base() = default;

    /**
     * The status of parsing.
     */
    enum class result {
        good,
        indeterminate,
        bad_method,
        bad_uri,
        bad_protocol,
        bad_version,
        bad_header,
        bad_end_of_headers,
        bad_status_code,
        bad_reason_phrase,
    };

    /**
     * Parses input and feeds the output to given request.
     * @tparam Iterator The interator type.
     * @param begin The beginning of the input.
     * @param end The end of the input.
     * @return A tuple with a result indicating the status, and an iterator indicating where the parsing stopped.
     */
    template<class Iterator>
    std::tuple<result, Iterator> parse(Iterator begin, Iterator end) {
        RAV_ASSERT(begin <= end, "Invalid input iterators");

        while (begin < end) {
            if (remaining_expected_data_ > 0) {
                const auto max_data = std::min(remaining_expected_data_, static_cast<long>(end - begin));
                data().insert(data().end(), begin, begin + max_data);
                remaining_expected_data_ -= max_data;
                begin += max_data;
                if (remaining_expected_data_ == 0) {
                    return std::make_tuple(result::good, begin);  // Reached the end of data, which means we are done
                }
                if (remaining_expected_data_ > 0) {
                    return std::make_tuple(result::indeterminate, begin);  // Need more data
                }
                RAV_ASSERT_FALSE("remaining_expected_data_ is negative, which should never happen");
                return std::make_tuple(result::bad_header, begin);
            }

            RAV_ASSERT(begin < end, "Expecting to have data available at this point");
            RAV_ASSERT(remaining_expected_data_ == 0, "No remaining data should be expected at this point");

            while (begin < end) {
                result result = consume(*begin++);

                if (result == result::good) {
                    // Find out how much data we should get
                    if (const auto data_length = get_content_length(); data_length.has_value()) {
                        remaining_expected_data_ = *data_length;
                    } else {
                        remaining_expected_data_ = 0;
                    }

                    if (remaining_expected_data_ > 0) {
                        break;  // Break out of this loop into outer loop to consume data
                    }

                    RAV_ASSERT(begin == end, "Expecting no more data left at this point");

                    return std::make_tuple(result, begin);
                }

                if (result != result::indeterminate) {
                    return std::make_tuple(result, begin);  // Error
                }
            }
        }

        return std::make_tuple(result::indeterminate, begin);
    }

    /**
     * Resets the parser to its initial state. Subclasses should also reset the pointed at request or response.
     */
    virtual void reset() {
        remaining_expected_data_ = {};
    }

protected:
    /**
     * Should be overridden by subclass to provide access to the data container.
     * @returns The data that has been parsed so far.
     */
    virtual std::string& data() = 0;

    /**
     * Should be overridden by the subclass to provide the content length of the data that is expected to be received.
     * @returns The content length of the data that is expected to be received.
     */
    [[nodiscard]] virtual std::optional<long> get_content_length() const = 0;

    /**
     * Should be overridden by the subclass to consume a character.
     * @param input The character to consume.
     * @return The result of consuming the character.
     */
    virtual result consume(char input) = 0;

    /// Check if a byte is an HTTP character.
    static bool is_char(const int c) {
        return c >= 0 && c <= 127;
    }

    /// Check if a byte is an HTTP control character.
    static bool is_ctl(const int c) {
        return (c >= 0 && c <= 31) || c == 127;
    }

    /// Check if a byte is defined as an HTTP tspecial character.
    static bool is_tspecial(const int c) {
        switch (c) {
            case '(':
            case ')':
            case '<':
            case '>':
            case '@':
            case ',':
            case ';':
            case ':':
            case '\\':
            case '"':
            case '/':
            case '[':
            case ']':
            case '?':
            case '=':
            case '{':
            case '}':
            case ' ':
            case '\t':
                return true;
            default:
                return false;
        }
    }

    /// Check if a byte is a digit.
    static bool is_digit(const int c) {
        return c >= '0' && c <= '9';
    }

  private:
    long remaining_expected_data_ {};
};

}  // namespace rav
