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
    int status_code{};
    std::string reason_phrase;
    int rtsp_version_major{};
    int rtsp_version_minor{};
    rtsp_headers headers;
    std::string data;

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
};

}
