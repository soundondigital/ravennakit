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

#include "rtsp_headers.hpp"

#include <string>

namespace rav {

/**
 * Structure that represents an RTSP response.
 */
struct rtsp_response {
    int status_code {};
    std::string reason_phrase;
    int rtsp_version_major {1};
    int rtsp_version_minor {0};
    rtsp_headers headers;
    std::string data;

    rtsp_response() = default;
    rtsp_response(int status, const std::string& reason);
    rtsp_response(int status, std::string reason, std::string data_);

    /**
     * Resets the request to its initial state.
     */
    void reset() {
        status_code = {};
        reason_phrase.clear();
        rtsp_version_major = {};
        rtsp_version_minor = {};
        headers.clear();
        data.clear();
    }

    /**
     * Encode the response into a string, meant for sending over the wire.
     * @param newline The newline character to use.
     * @return The encoded response as a string.
     */
    std::string encode(const char* newline = "\r\n") const;

    /**
     * Encoded the response into a string.
     * @param out The output string to append to.
     * @param newline The newline character to use.
     */
    void encode_append(std::string& out, const char* newline = "\r\n") const;

    /**
     * Convert the response to a debug string.
     * @return The response as a debug string.
     */
    [[nodiscard]] std::string to_debug_string(bool include_data) const;
};

}  // namespace rav
