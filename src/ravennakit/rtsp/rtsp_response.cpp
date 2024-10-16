/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtsp/rtsp_response.hpp"

std::string rav::rtsp_response::encode(const char* newline) {
    std::string out;
    encode_append(out, newline);
    return out;
}

void rav::rtsp_response::encode_append(std::string& out, const char* newline) {
    fmt::format_to(
        std::back_inserter(out), "RTSP/{}.{} {} {}{}", rtsp_version_major, rtsp_version_minor, status_code,
        reason_phrase, newline
    );
    if (!data.empty()) {
        headers.emplace_back({"Content-Length", std::to_string(data.size())});
    }
    headers.encode_append(out);
    out += newline;
    out += data;
}
