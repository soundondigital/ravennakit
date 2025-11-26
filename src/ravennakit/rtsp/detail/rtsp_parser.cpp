/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/detail/rtsp_parser.hpp"

rav::rtsp::Parser::result rav::rtsp::Parser::parse(StringBuffer& input) {
    while (!input.exhausted()) {
        if (state_ == state::start) {
            const auto start_line = input.read_until_newline();
            if (!start_line) {
                return result::indeterminate;
            }
            if (start_line->empty()) {
                return result::unexpected_blank_line;
            }
            start_line_ = *start_line;

            state_ = state::headers;
        }

        if (state_ == state::headers) {
            for (int i = 0; i < k_loop_upper_bound; ++i) {
                const auto header_line = input.read_until_newline();
                if (!header_line) {
                    return result::indeterminate;
                }

                // End of headers
                if (header_line->empty()) {
                    state_ = state::data;
                    break;
                }

                // Folded headers
                if (string_starts_with(*header_line, " ") || string_starts_with(*header_line, "\t")) {
                    if (headers_.empty()) {
                        return result::bad_header;
                    }
                    if (headers_.back().name.empty()) {
                        return result::bad_header;
                    }
                    headers_.back().value += header_line->substr(1);
                    continue;  // Next header line
                }

                Headers::Header h;
                StringParser header_parser(*header_line);

                // Header name
                if (auto name = header_parser.split(':')) {
                    h.name = *name;
                } else {
                    return result::bad_header;
                }

                if (!header_parser.skip(' ')) {
                    return result::bad_header;
                }

                // Header value
                if (auto value = header_parser.read_until_end()) {
                    h.value = *value;
                } else {
                    return result::bad_header;
                }

                headers_.emplace_back(std::move(h));
            }
        }

        if (state_ == state::data) {
            if (const auto length = headers_.get_content_length()) {
                if (length > 0) {
                    if (length > input.remaining()) {
                        return result::indeterminate;
                    }
                    data_ = input.read(*length);
                }
            }
            state_ = state::complete;
        }

        if (state_ == state::complete) {
            constexpr auto rtsp_response_prefix = std::string_view("RTSP/");

            if (string_starts_with(start_line_, rtsp_response_prefix)) {
                if (const auto r = handle_response(); r != result::good) {
                    return r;
                }
            } else {
                if (const auto r = handle_request(); r != result::good) {
                    return r;
                }
            }

            state_ = state::start;
        }
    }

    return result::good;
}

void rav::rtsp::Parser::reset() noexcept {
    on_request.reset();
    on_response.reset();
    state_ = state::start;
    start_line_.clear();
    headers_.clear();
    data_.clear();
    request_.clear();
    response_.clear();
}

rav::rtsp::Parser::result rav::rtsp::Parser::handle_response() {
    StringParser p(start_line_);
    p.skip("RTSP/");
    const auto version_major = p.read_int<int32_t>();
    if (!version_major) {
        return result::bad_version;
    }
    if (!p.skip('.')) {
        return result::bad_version;
    }
    const auto version_minor = p.read_int<int32_t>();
    if (!version_minor) {
        return result::bad_version;
    }
    if (!p.skip(' ')) {
        return result::bad_status_code;
    }
    const auto status_code = p.read_int<int32_t>();
    if (!status_code) {
        return result::bad_status_code;
    }
    if (!p.skip(' ')) {
        return result::bad_reason_phrase;
    }
    const auto reason_phrase = p.read_until_end();
    if (!reason_phrase) {
        return result::bad_reason_phrase;
    }

    // Assemble response
    response_.clear();
    response_.rtsp_version_major = *version_major;
    response_.rtsp_version_minor = *version_minor;
    response_.status_code = *status_code;
    response_.reason_phrase = *reason_phrase;

    RAV_ASSERT(response_.rtsp_headers.empty(), "RTSP headers should be empty");
    RAV_ASSERT(response_.data.empty(), "RTSP data should be empty");

    std::swap(response_.rtsp_headers, headers_);
    std::swap(response_.data, data_);

    on_response(response_);

    return result::good;
}

rav::rtsp::Parser::result rav::rtsp::Parser::handle_request() {
    StringParser p(start_line_);
    const auto method = p.split(' ');
    if (!method) {
        return result::bad_method;
    }
    const auto uri = p.split(' ');
    if (!uri) {
        return result::bad_uri;
    }
    if (!p.skip("RTSP/")) {
        return result::bad_protocol;
    }
    const auto version_major = p.read_int<int32_t>();
    if (!version_major) {
        return result::bad_version;
    }
    if (!p.skip('.')) {
        return result::bad_version;
    }
    const auto version_minor = p.read_int<int32_t>();
    if (!version_minor) {
        return result::bad_version;
    }

    // Assemble request
    request_.clear();
    request_.method = *method;
    request_.uri = *uri;
    request_.rtsp_version_major = *version_major;
    request_.rtsp_version_minor = *version_minor;

    RAV_ASSERT(request_.rtsp_headers.empty(), "RTSP headers should be empty");
    RAV_ASSERT(request_.data.empty(), "RTSP data should be empty");

    std::swap(request_.rtsp_headers, headers_);
    std::swap(request_.data, data_);

    on_request(request_);

    return result::good;
}
