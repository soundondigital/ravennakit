/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/util/uri.hpp"

#include "ravennakit/core/string_parser.hpp"

rav::uri rav::uri::parse(const std::string& encoded_uri) {
    uri uri {};
    string_parser parser {encoded_uri};

    // Scheme (required)
    const auto scheme = parser.read_until("://");
    if (!scheme) {
        RAV_THROW_EXCEPTION("Invalid URI scheme");
    }
    uri.scheme = decode(*scheme);

    // Authority (optional)
    if (const auto authority = parser.split("/")) {
        string_parser authority_parser {*authority};
        // User info (optional)
        if (const auto user_info = authority_parser.read_until("@")) {
            string_parser user_info_parser {*user_info};
            if (const auto user = user_info_parser.split(":")) {
                uri.user = decode(*user);
            }
            if (const auto password = user_info_parser.read_until_end()) {
                uri.password = decode(*password);
            }
        }
        // Host (optional)
        if (const auto host = authority_parser.split(":")) {
            uri.host = decode(*host);
        }
        // Port (optional)
        if (const auto port = authority_parser.read_int<uint16_t>()) {
            uri.port = *port;
        }
    }

    // Path (required)
    if (const auto path1 = parser.read_until("?")) {
        uri.path = decode(fmt::format("/{}", *path1));
        // Query (optional)
        if (const auto query = parser.split("#")) {
            uri.query = parse_query(*query);
        }
        // Fragment (optional)
        if (const auto fragment = parser.read_until_end()) {
            uri.fragment = decode(*fragment);
        }
    } else if (const auto path2 = parser.read_until("#")) {
        uri.path = decode(fmt::format("/{}", *path2));
        // Fragment (optional)
        if (const auto fragment = parser.read_until_end()) {
            uri.fragment = decode(*fragment);
        }
    } else if (const auto path3 = parser.read_until_end()) {
        uri.path = decode(fmt::format("/{}", *path3));
    }

    return uri;
}

std::string rav::uri::to_string() const {
    std::string output;
    output.reserve(256);

    // Scheme
    output.append(encode(scheme));
    output.append("://");

    // User info
    bool encode_at = false;
    if (!user.empty()) {
        output.append(encode(user));
        encode_at = true;
    }
    if (!password.empty()) {
        output.append(":");
        output.append(encode(password));
        encode_at = true;
    }
    if (encode_at) {
        output.push_back('@');
    }

    // Host
    output.append(encode(host));

    // Port
    if (port.has_value()) {
        output.append(":");
        output.append(std::to_string(*port));
    }

    // Path
    if (path.empty()) {
        output.append("/");
    } else {
        output.append(encode(path));
    }

    // Query
    if (!query.empty()) {
        output.append("?");
        for (auto& [key, value] : query) {
            output.append(encode(key, true, true));
            output.append("=");
            output.append(encode(value, true, true));
            output.append("&");
        }
        output.pop_back(); // Remove last '&'
    }

    // Fragment
    if (!fragment.empty()) {
        output.append("#");
        output.append(encode(fragment));
    }

    return output;
}

std::string rav::uri::encode(const std::string_view str, const bool encode_plus, const bool encode_slash) {
    std::string output;
    output.reserve(str.size());
    for (const auto chr : str) {
        if (std::isalnum(chr) || chr == '-' || chr == '_' || chr == '.' || chr == '~'
            || (!encode_slash && chr == '/')) {
            output.push_back(chr);
        } else if (encode_plus && chr == ' ') {
            output.push_back('+');
        } else {
            output.push_back('%');
            output.push_back("0123456789ABCDEF"[static_cast<uint8_t>(chr) >> 4]);
            output.push_back("0123456789ABCDEF"[static_cast<uint8_t>(chr) & 0x0F]);
        }
    }
    return output;
}

std::string rav::uri::encode(const std::string_view scheme, const std::string_view host, const std::string_view path) {
    return fmt::format("{}://{}{}", encode(scheme), host, encode(path));
}

std::string rav::uri::decode(const std::string_view encoded, const bool decode_plus) {
    std::string output;
    output.reserve(encoded.size());
    for (size_t i = 0; i < encoded.size(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.size()) {
            const auto hex = encoded.substr(i + 1, 2);
            const auto value = std::stoi(std::string(hex), nullptr, 16);
            output.push_back(static_cast<char>(value));
            i += 2;
        } else if (decode_plus && encoded[i] == '+') {
            output.push_back(' ');
        } else {
            output.push_back(encoded[i]);
        }
    }
    return output;
}

std::map<std::string, std::string> rav::uri::parse_query(const std::string_view query_string) {
    std::map<std::string, std::string> query;
    string_parser parser {query_string};
    parser.skip('?');
    while (!parser.exhausted()) {
        if (const auto key = parser.read_until("=")) {
            auto& map_value = query[decode(*key, true)];
            if (const auto value = parser.split("&")) {
                map_value = decode(*value, true);
            }
        } else {
            RAV_THROW_EXCEPTION("Invalid query string");
        }
    }

    return query;
}
