/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/detail/rtsp_response.hpp"

rav::rtsp::Response::Response(const int status, const std::string& reason) : Response(status, reason, {}) {}

rav::rtsp::Response::Response(const int status, std::string reason, std::string data_) :
    status_code(status), reason_phrase(std::move(reason)), data(std::move(data_)) {}

std::string rav::rtsp::Response::encode(const char* newline) const {
    std::string out;
    encode_append(out, newline);
    return out;
}

void rav::rtsp::Response::encode_append(std::string& out, const char* newline) const {
    fmt::format_to(
        std::back_inserter(out), "RTSP/{}.{} {} {}{}", rtsp_version_major, rtsp_version_minor, status_code,
        reason_phrase, newline
    );
    rtsp_headers.encode_append(out, true);
    if (!data.empty()) {
        fmt::format_to(std::back_inserter(out), "content-length: {}{}", data.size(), newline);
    }
    out += newline;
    out += data;
}

std::string rav::rtsp::Response::to_debug_string(const bool include_data) const {
    std::string out;
    fmt::format_to(
        std::back_inserter(out), "RTSP/{}.{} {} {}", rtsp_version_major, rtsp_version_minor, status_code, reason_phrase
    );
    out += rtsp_headers.to_debug_string();
    if (include_data && !data.empty()) {
        out += "\n";
        out += string_replace(data, "\r\n", "\n");
    }
    return out;
}
