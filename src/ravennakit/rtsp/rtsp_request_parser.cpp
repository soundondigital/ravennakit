/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/rtsp_request_parser.hpp"

rav::rtsp_request_parser::result rav::rtsp_request_parser::consume(const char c) {
    fmt::print("{}", c);

    switch (state_) {
        case state::method_start:
            if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                return result::bad_method;
            }
            state_ = state::method;
            request_.method.push_back(c);
            return result::indeterminate;
        case state::method:
            if (c == ' ') {
                state_ = state::uri;
                return result::indeterminate;
            }
            if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                return result::bad_method;
            }
            request_.method.push_back(c);
            return result::indeterminate;
        case state::uri:
            if (c == ' ') {
                state_ = state::rtsp_r;
                return result::indeterminate;
            }
            if (is_ctl(c)) {
                return result::bad_uri;
            }
            request_.uri.push_back(c);
            return result::indeterminate;
        case state::rtsp_r:
            if (c != 'R') {
                return result::bad_protocol;
            }
            state_ = state::rtsp_t;
            return result::indeterminate;
        case state::rtsp_t:
            if (c != 'T') {
                return result::bad_protocol;
            }
            state_ = state::rtsp_s;
            return result::indeterminate;
        case state::rtsp_s:
            if (c != 'S') {
                return result::bad_protocol;
            }
            state_ = state::rtsp_p;
            return result::indeterminate;
        case state::rtsp_p:
            if (c != 'P') {
                return result::bad_protocol;
            }
            state_ = state::rtsp_slash;
            return result::indeterminate;
        case state::rtsp_slash:
            if (c != '/') {
                return result::bad_protocol;
            }
            state_ = state::version_major;
            return result::indeterminate;
        case state::version_major:
            if (c != '1') {
                return result::bad_version;
            }
            request_.rtsp_version_major = 1;
            state_ = state::version_dot;
            return result::indeterminate;
        case state::version_dot:
            if (c != '.') {
                return result::bad_version;
            }
            state_ = state::version_minor;
            return result::indeterminate;
        case state::version_minor:
            if (c != '0') {
                return result::bad_version;
            }
            request_.rtsp_version_minor = 0;
            state_ = state::expecting_newline_1;
            return result::indeterminate;
        case state::header_value_newline:
            if (c == '\r' || c == '\n') {
                if (c == previous_c_) {
                    return result::good;
                }
                return result::indeterminate; // Or good, depending on whether all data is consumed.
            }
        case state::expecting_newline_1:
            if (c == '\n') {
                state_ = state::header_start;
                return result::indeterminate;
            }
            if (c == '\r') {
                if (previous_c_ == c) {
                    return result::good;
                }
                state_ = state::expecting_newline_1;
                return result::indeterminate;
            }
            state_ = state::header_start;
        case state::header_start:
            if (c == '\r' || c == '\n') {
                state_ = state::end_of_headers;
                return result::indeterminate;
            }
            if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                return result::bad_header;
            }
            state_ = state::header_name;
            request_.headers.emplace_back().name.push_back(c);
            return result::indeterminate;
        case state::header_name:
            if (c == ':') {
                state_ = state::space_before_header_value;
                return result::indeterminate;
            }
            if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                return result::bad_header;
            }
            request_.headers.back().name.push_back(c);
            return result::indeterminate;
        case state::space_before_header_value:
            if (c == ' ') {
                return result::indeterminate;
            }
            state_ = state::header_value;
        case state::header_value:
            if (c == '\r' || c == '\n') {
                state_ = state::header_value_newline;
                return result::indeterminate;
            }
            if (is_ctl(c)) {
                return result::bad_header;
            }
            request_.headers.back().value.push_back(c);
            return result::indeterminate;

        case state::end_of_headers:
            if (c == '\r') {
                state_ = state::end_of_headers;
                return result::indeterminate;
            }
            if (c == '\n') {
                return result::good;
            }
            return result::bad_end_of_headers;
    }
    return result::indeterminate;
}

bool rav::rtsp_request_parser::is_char(const int c) {
    return c >= 0 && c <= 127;
}

bool rav::rtsp_request_parser::is_ctl(const int c) {
    return (c >= 0 && c <= 31) || c == 127;
}

bool rav::rtsp_request_parser::is_tspecial(const int c) {
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

bool rav::rtsp_request_parser::is_digit(const int c) {
    return c >= '0' && c <= '9';
}
