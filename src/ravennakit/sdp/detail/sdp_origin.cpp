/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_origin.hpp"
#include "ravennakit/core/string_parser.hpp"
#include "ravennakit/sdp/detail/sdp_constants.hpp"

tl::expected<void, std::string> rav::sdp::OriginField::validate() const {
    if (session_id.empty()) {
        return tl::unexpected("origin: session id is empty");
    }

    if (unicast_address.empty()) {
        return tl::unexpected("origin: unicast address is empty");
    }

    if (network_type == NetwType::undefined) {
        return tl::unexpected("origin: network type is undefined");
    }

    if (address_type == AddrType::undefined) {
        return tl::unexpected("origin: address type is undefined");
    }

    return {};
}

tl::expected<std::string, std::string> rav::sdp::OriginField::to_string() const {
    auto result = validate();
    if (!result) {
        return tl::unexpected(result.error());
    }

    return fmt::format(
        "o={} {} {} {} {} {}", username.empty() ? "-" : username, session_id, session_version,
        sdp::to_string(network_type), sdp::to_string(address_type), unicast_address
    );
}

rav::sdp::OriginField::ParseResult<rav::sdp::OriginField> rav::sdp::OriginField::parse_new(std::string_view line) {
    string_parser parser(line);

    if (!parser.skip("o=")) {
        return ParseResult<OriginField>::err("origin: expecting 'o='");
    }

    OriginField o;

    // Username
    if (const auto username = parser.split(' ')) {
        o.username = *username;
    } else {
        return ParseResult<OriginField>::err("origin: failed to parse username");
    }

    // Session id
    if (const auto session_id = parser.split(' ')) {
        o.session_id = *session_id;
    } else {
        return ParseResult<OriginField>::err("origin: failed to parse session id");
    }

    // Session version
    if (const auto version = parser.read_int<int32_t>()) {
        o.session_version = *version;
        parser.skip(' ');
    } else {
        return ParseResult<OriginField>::err("origin: failed to parse session version");
    }

    // Network type
    if (const auto network_type = parser.split(' ')) {
        if (*network_type != k_sdp_inet) {
            return ParseResult<OriginField>::err("origin: invalid network type");
        }
        o.network_type = NetwType::internet;
    } else {
        return ParseResult<OriginField>::err("origin: failed to parse network type");
    }

    // Address type
    if (const auto address_type = parser.split(' ')) {
        if (*address_type == k_sdp_ipv4) {
            o.address_type = AddrType::ipv4;
        } else if (*address_type == k_sdp_ipv6) {
            o.address_type = AddrType::ipv6;
        } else {
            return ParseResult<OriginField>::err("origin: invalid address type");
        }
    } else {
        return ParseResult<OriginField>::err("origin: failed to parse address type");
    }

    // Address
    if (const auto address = parser.split(' ')) {
        o.unicast_address = *address;
    } else {
        return ParseResult<OriginField>::err("origin: failed to parse address");
    }

    return ParseResult<OriginField>::ok(std::move(o));
}
