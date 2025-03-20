/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/detail/rtsp_request.hpp"

#include "ravennakit/core/uri.hpp"

#include <catch2/catch_all.hpp>

void rav::rtsp::Request::reset() {
    method.clear();
    uri.clear();
    rtsp_version_major = {};
    rtsp_version_minor = {};
    rtsp_headers.clear();
    data.clear();
}

std::string rav::rtsp::Request::encode(const char* newline) const {
    std::string out;
    encode_append(out, newline);
    return out;
}

void rav::rtsp::Request::encode_append(std::string& out, const char* newline) const {
    fmt::format_to(
        std::back_inserter(out), "{} {} RTSP/{}.{}{}", method, uri, rtsp_version_major, rtsp_version_minor, newline
    );
    rtsp_headers.encode_append(out, true);
    if (!data.empty()) {
        fmt::format_to(std::back_inserter(out), "content-length: {}{}", data.size(), newline);
    }
    out += newline;
    out += data;
}

std::string rav::rtsp::Request::to_debug_string(bool include_data) const {
    std::string out;
    fmt::format_to(std::back_inserter(out), "{} {} RTSP/{}.{}", method, uri, rtsp_version_major, rtsp_version_minor);
    out += rtsp_headers.to_debug_string();
    if (include_data && !data.empty()) {
        out += "\n";
        out += string_replace(data, "\r\n", "\n");
    }
    return out;
}
