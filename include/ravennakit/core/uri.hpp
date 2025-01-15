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

#include <map>
#include <string>
#include <optional>
#include <cstdint>

namespace rav {

/**
 * URI parser and encoder. Should be enough for the common case, but it's not a full implementation.
 */
struct uri {
    std::string scheme;
    std::string user;
    std::string password;
    std::string host;
    std::optional<uint16_t> port;
    std::string path;
    std::map<std::string, std::string> query;
    std::string fragment;

    /**
     * Parse a string into a URI.
     * @param encoded_uri Encoded URI string.
     * @return Parsed URI.
     */
    static uri parse(const std::string& encoded_uri);

    /**
     * Encodes the URI into a string.
     * @return Encoded URI string.
     */
    [[nodiscard]] std::string to_string() const;

    /**
     * Encode a string to be used in a URI.
     * @param str String to encode.
     * @param encode_plus Whether to encode '+' as '%2B'.
     * @param encode_slash Whether to encode '/' as '%2F'.
     * @return Encoded string.
     */
    static std::string encode(std::string_view str, bool encode_plus = false, bool encode_slash = false);

    /**
     * Shortcut to create a basic encoded URI.
     * @param scheme Scheme.
     * @param host Host.
     * @param path Path.
     * @return Encoded URI string.
     */
    static std::string encode(std::string_view scheme, std::string_view host, std::string_view path);

    /**
     * Decodes a percent-encoded string.
     * @param encoded Encoded string.
     * @param decode_plus Whether to decode '+' as space.
     * @return Decoded string.
     */
    [[nodiscard]] static std::string decode(std::string_view encoded, bool decode_plus = false);

    /**
     * Parse a query string into a map.
     * @return Map of query parameters.
     */
    static std::map<std::string, std::string> parse_query(std::string_view);
};

}  // namespace rav
