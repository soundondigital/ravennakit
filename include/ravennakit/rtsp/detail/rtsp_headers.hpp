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
#include "ravennakit/core/format.hpp"

namespace rav::rtsp {

/**
 * Class for holding RTSP headers.
 */
class Headers {
  public:
    /**
     * Struct for holding a header name and value.
     */
    struct Header {
        std::string name;
        std::string value;
    };

    /**
     * Finds a header by name and returns its value. The name is case-insensitive.
     * @param name The name of the header.
     * @return The value of the header if found, otherwise nullptr.
     */
    [[nodiscard]] const Header* get(const std::string& name) const {
        for (auto& h : headers_) {
            if (string_compare_case_insensitive(h.name, name)) {
                return &h;
            }
        }
        return nullptr;
    }

    /**
     * Finds a header by name and returns its value. The name is case-insensitive. If the header doesn't exist, the
     * default value is returned.
     * @param name The name of the header.
     * @return The value of the header if found, otherwise the default value.
     */
    [[nodiscard]] std::string get_or_default(const std::string& name) const {
        for (auto& h : headers_) {
            if (string_compare_case_insensitive(h.name, name)) {
                return h.value;
            }
        }
        return {};
    }

    /**
     * @returns Tries to find the Content-Length header and returns its value as integer.
     */
    [[nodiscard]] std::optional<size_t> get_content_length() const {
        if (const auto* h = get("content-length"); h) {
            return rav::ston<size_t>(h->value);
        }
        return std::nullopt;
    }

    /**
     * Retrieves the header at the specified index. Bounds are not checked, be careful.
     * @param index The index of the header.
     * @return The header at the specified index.
     */
    Header& operator[](const size_t index) {
        return headers_[index];
    }

    /**
     * Retrieves the header at the specified index. Bounds are not checked, be careful.
     * @param index The index of the header.
     * @return The header at the specified index.
     */
    [[nodiscard]] const Header& operator[](const size_t index) const {
        return headers_[index];
    }

    /**
     * @return The headers.
     */
    [[nodiscard]] const std::vector<Header>& get_headers() const {
        return headers_;
    }

    /**
     * Sets a header value. If the header does not exist, it will be created.
     * @param name The name of the header.
     * @param value The value of the header.
     */
    void set(const char* name, const char* value) {
        for (auto& h : headers_) {
            if (string_compare_case_insensitive(h.name, name)) {
                h.value = value;
                return;
            }
        }
        headers_.push_back({name, value});
    }

    /**
     * Sets a header value. If the header does not exist, it will be created.
     * @param name The name of the header.
     * @param value The value of the header.
     */
    void set(const std::string& name, std::string value) {
        for (auto& h : headers_) {
            if (string_compare_case_insensitive(h.name, name)) {
                h.value = std::move(value);
                return;
            }
        }
        headers_.push_back({name, std::move(value)});
    }

    /**
     * Sets a header value, updating the value if the header already exists, or creating it if it does not.
     * @param new_header The header to set.
     */
    void set(const Header& new_header) {
        for (auto& h : headers_) {
            if (string_compare_case_insensitive(h.name, new_header.name)) {
                h.value = new_header.value;
                return;
            }
        }
        headers_.push_back({new_header.name, new_header.value});
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
     * Adds given header to the end.
     * @param new_header The header to add.
     */
    void push_back(Header new_header) {
        for (auto& h : headers_) {
            if (string_compare_case_insensitive(h.name, new_header.name)) {
                h.value = std::move(new_header.value);
                return;
            }
        }
        headers_.push_back(std::move(new_header));
    }

    /**
     * Adds an empty header at the end of the array.
     * @returns The newly added header.
     */
    Header& emplace_back() {
        return headers_.emplace_back();
    }

    /**
     * Adds given header at the end of the array.
     * @param new_header The header to add.
     * @return The newly added header.
     */
    Header& emplace_back(Header&& new_header) {
        for (auto& h : headers_) {
            if (string_compare_case_insensitive(h.name, new_header.name)) {
                h.value = std::move(new_header.value);
                return h;
            }
        }
        return headers_.emplace_back(std::move(new_header));
    }

    /**
     * @returns The last header.
     */
    Header& back() {
        return headers_.back();
    }

    /**
     * Encodes the current headers in a series of key: value\r\n lines.
     * If there is a Content-Length header, it will be skipped.
     * @param output The string to append to.
     * @param skip_content_length If true, a Content-Length header will be skipped, if it exists.
     */
    void encode_append(std::string& output, const bool skip_content_length) const {
        for (const auto& h : headers_) {
            if (skip_content_length && string_compare_case_insensitive(h.name, "content-length")) {
                continue;
            }
            fmt::format_to(std::back_inserter(output), "{}: {}\r\n", h.name, h.value);
        }
    }

    /**
     * Returns headers as a string. Each header is on a new line. Meant for debugging. For encoding into a buffer, use
     * encode_append.
     * @return The headers as a string.
     */
    [[nodiscard]] std::string to_debug_string() const {
        std::string out;
        for (const auto& h : headers_) {
            fmt::format_to(std::back_inserter(out), ", {}: {}", h.name, h.value);
        }
        return out;
    }

  private:
    std::vector<Header> headers_;
};

}  // namespace rav
