/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/rtsp_response_parser.hpp"

rav::rtsp_parser_base::result rav::rtsp_response_parser::consume(const char c) {
    switch (state_) {
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
            state_ = state::rtsp_1;
            return result::indeterminate;
        case state::rtsp_1:
            if (c != '1') {
                return result::bad_version;
            }
            response_.rtsp_version_major = 1;
            state_ = state::rtsp_dot;
            return result::indeterminate;
        case state::rtsp_dot:
            if (c != '.') {
                return result::bad_version;
            }
            state_ = state::rtsp_0;
            return result::indeterminate;
        case state::rtsp_0:
            if (c != '0') {
                return result::bad_version;
            }
            response_.rtsp_version_minor = 0;
            state_ = state::rtsp_space;
            return result::indeterminate;
        case state::rtsp_space:
            if (c != ' ') {
                return result::bad_version;
            }
            state_ = state::status_code_0;
            return result::indeterminate;
        case state::status_code_0:
            if (!is_digit(c)) {
                return result::bad_status_code;
            }
            response_.status_code += (c - '0') * 100;
            state_ = state::status_code_1;
            return result::indeterminate;
        case state::status_code_1:
            if (!is_digit(c)) {
                return result::bad_status_code;
            }
            response_.status_code += (c - '0') * 10;
            state_ = state::status_code_2;
            return result::indeterminate;
        case state::status_code_2:
            if (!is_digit(c)) {
                return result::bad_status_code;
            }
            response_.status_code += c - '0';
            state_ = state::status_code_space;
            return result::indeterminate;
        case state::status_code_space:
            if (c != ' ') {
                return result::bad_status_code;
            }
            state_ = state::reason_phrase;
            return result::indeterminate;
        case state::reason_phrase:
            if (c == '\r') {
                state_ = state::reason_phrase;
                return result::indeterminate;
            }
            if (c == '\n') {
                state_ = state::header_start;
                return result::indeterminate;
            }
            if (!is_char(c) && !is_ctl(c) && !is_tspecial(c)) {
                return result::bad_reason_phrase;
            }
            response_.reason_phrase.push_back(c);
            return result::indeterminate;
        case state::header_start:
            if (c == ' ' || c == '\t') {
                // Folded header
                state_ = state::header_value;
                return result::indeterminate;
            }
            if (c == '\r') {
                state_ = state::header_start;
                return result::indeterminate;
            }
            if (c == '\n') {
                return result::good;
            }
            if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                return result::bad_header;
            }
            state_ = state::header_name;
            response_.headers.emplace_back().name.push_back(c);
            return result::indeterminate;
        case state::header_name:
            if (c == ':') {
                state_ = state::space_before_header_value;
                return result::indeterminate;
            }
            if (!is_char(c) || is_ctl(c) || is_tspecial(c)) {
                return result::bad_header;
            }
            response_.headers.back().name.push_back(c);
            return result::indeterminate;
        case state::space_before_header_value:
            if (c == ' ') {
                return result::indeterminate;
            }
            state_ = state::header_value;
        case state::header_value:
            if (c == '\r') {
                state_ = state::header_value;
                return result::indeterminate;
            }
            if (c == '\n') {
                state_ = state::header_start;
                return result::indeterminate;
            }
            if (is_ctl(c)) {
                return result::bad_header;
            }
            response_.headers.back().value.push_back(c);
            return result::indeterminate;
    }

    return result::indeterminate;
}
