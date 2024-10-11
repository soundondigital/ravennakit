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

#include "rtsp_request.hpp"

namespace rav {

class rtsp_request_parser {
  public:
    enum class result {
        good,
        indeterminate,
        more_data_needed,
        bad_method,
        bad_uri,
        bad_protocol,
        bad_version,
        bad_header,
        bad_end_of_headers
    };

    explicit rtsp_request_parser(rtsp_request& request) : request_(request) {}

    template<typename InputIterator>
    std::tuple<result, InputIterator> parse(InputIterator begin, InputIterator end) {
        while (begin != end) {
            if (result result = consume(*begin++); result != result::indeterminate) {
                return std::make_tuple(result, begin);
            }
            previous_c_ = *begin;
        }
        return std::make_tuple(result::indeterminate, begin);
    }

  private:
    enum class state {
        method_start,
        method,
        uri,
        rtsp_r,
        rtsp_t,
        rtsp_s,
        rtsp_p,
        rtsp_slash,
        version_major,
        version_dot,
        version_minor,
        expecting_newline_1,
        header_start,
        header_name,
        space_before_header_value,
        header_value,
        header_value_newline,
        end_of_headers,
    };

    rtsp_request& request_;
    state state_ {state::method_start};
    char previous_c_ {0};

    result consume(char c);

    /// Check if a byte is an HTTP character.
    static bool is_char(int c);

    /// Check if a byte is an HTTP control character.
    static bool is_ctl(int c);

    /// Check if a byte is defined as an HTTP tspecial character.
    static bool is_tspecial(int c);

    /// Check if a byte is a digit.
    static bool is_digit(int c);
};

}  // namespace rav
