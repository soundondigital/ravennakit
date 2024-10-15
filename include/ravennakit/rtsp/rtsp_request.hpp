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
#include "ravennakit/core/string_parser.hpp"

#include <string>

namespace rav {

/**
 * Structure that represents an RTSP request.
 */
struct rtsp_request {
    std::string method;
    std::string uri;
    int rtsp_version_major{};
    int rtsp_version_minor{};
    rtsp_headers headers;
    std::string data;

    /**
     * Resets the request to its initial state.
     */
    void reset();

    /**
     * @return The encoded request as a string.
     */
    std::string encode();

    /**
     * Encoded the request into a string.
     * @param out The output string to append to.
     */
    void encode_append(std::string& out);
};

}  // namespace rav
