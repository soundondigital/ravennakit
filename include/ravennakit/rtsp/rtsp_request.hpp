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

#include "ravennakit/core/string_parser.hpp"

#include <string>
#include <tl/expected.hpp>

namespace rav {

class rtsp_request {
public:
    struct header {
        std::string name;
        std::string value;
    };

    std::string method;
    std::string uri;
    int rtsp_version_major;
    int rtsp_version_minor;
    std::vector<header> headers;

    [[nodiscard]] const std::string* get_header(const std::string& name) const {
        for (const auto& header : headers) {
            if (header.name == name) {
                return &header.value;
            }
        }
        return nullptr;
    }
};

}  // namespace rav
