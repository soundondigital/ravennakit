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

#include "ravennakit/core/string.hpp"

#include <string>
#include <vector>

namespace rav {

/**
 * Class for holding RTSP headers.
 */
class rtsp_headers {
  public:
    /**
     * Struct for holding a header name and value.
     */
    struct header {
        std::string name;
        std::string value;
    };

    /**
     * Finds a header by name and returns its value.
     * @param name The name of the header.
     * @returns The value of the header if found, otherwise nullptr.
     */
    [[nodiscard]] const std::string* get_header_value(const std::string& name) const {
        for (const auto& header : headers_) {
            if (header.name == name) {
                return &header.value;
            }
        }
        return nullptr;
    }

    /**
     * @returns Tries to find the Content-Length header and returns its value as integer.
     */
    [[nodiscard]] std::optional<long> get_content_length() const {
        if (const std::string* content_length = get_header_value("Content-Length"); content_length) {
            return rav::ston<long>(*content_length);
        }
        return std::nullopt;
    }

    /**
     * Retrieves the header at the specified index. Bounds are not checked, be careful.
     * @param index The index of the header.
     * @return The header at the specified index.
     */
    header& operator[](const size_t index) {
        return headers_[index];
    }

    /**
     * Retrieves the header at the specified index. Bounds are not checked, be careful.
     * @param index The index of the header.
     * @return The header at the specified index.
     */
    [[nodiscard]] const header& operator[](const size_t index) const {
        return headers_[index];
    }

    /**
     * Finds a header by name and returns its value. If header is not found, an empty string will be returned.
     * @param name The name of the header.
     * @return The value of the header if found, otherwise an empty string.
     */
    std::string operator[](const char* name) const {
        if (const std::string* value = get_header_value(name); value) {
            return *value;
        }
        return "";
    }

    /**
     * @return The headers.
     */
    [[nodiscard]] const std::vector<header>& headers() const {
        return headers_;
    }

    /**
     * Clears all headers.
     */
    void clear() {
        headers_.clear();
    }

    /**
     * @returns True if there are no headers.
     */
    [[nodiscard]] bool empty() const {
        return headers_.empty();
    }

    /**
     * @returns The number of headers.
     */
    [[nodiscard]] size_t size() const {
        return headers_.size();
    }

    /**
     * @param header The header to add.
     */
    void push_back(header header) {
        for (auto& h : headers_) {
            if (h.name == header.name) {
                h.value = std::move(header.value);
                return;
            }
        }
        headers_.push_back(std::move(header));
    }

    /**
     * Adds an empty header at the end of the array.
     * @returns The newly added header.
     */
    header& emplace_back() {
        return headers_.emplace_back();
    }

    /**
     * Adds given header at the end of the array.
     * @param header The header to add.
     * @return The newly added header.
     */
    header& emplace_back(header&& header) {
        for (auto& h : headers_) {
            if (h.name == header.name) {
                h.value = std::move(header.value);
                return h;
            }
        }
        return headers_.emplace_back(std::move(header));
    }

    /**
     * @returns The last header.
     */
    header& back() {
        return headers_.back();
    }

  private:
    std::vector<header> headers_;
};

}  // namespace rav
