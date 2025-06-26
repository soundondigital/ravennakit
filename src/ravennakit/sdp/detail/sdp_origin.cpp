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

tl::expected<rav::sdp::OriginField, std::string> rav::sdp::parse_origin(std::string_view line) {
    StringParser parser(line);

    if (!parser.skip("o=")) {
        return tl::unexpected("origin: expecting 'o='");
    }

    OriginField o;

    // Username
    if (const auto username = parser.split(' ')) {
        o.username = *username;
    } else {
        return tl::unexpected("origin: failed to parse username");
    }

    // Session id
    if (const auto session_id = parser.split(' ')) {
        o.session_id = *session_id;
    } else {
        return tl::unexpected("origin: failed to parse session id");
    }

    // Session version
    if (const auto version = parser.read_int<int32_t>()) {
        o.session_version = *version;
        parser.skip(' ');
    } else {
        return tl::unexpected("origin: failed to parse session version");
    }

    // Network type
    if (const auto network_type = parser.split(' ')) {
        if (*network_type != k_sdp_inet) {
            return tl::unexpected("origin: invalid network type");
        }
        o.network_type = NetwType::internet;
    } else {
        return tl::unexpected("origin: failed to parse network type");
    }

    // Address type
    if (const auto address_type = parser.split(' ')) {
        if (*address_type == k_sdp_ipv4) {
            o.address_type = AddrType::ipv4;
        } else if (*address_type == k_sdp_ipv6) {
            o.address_type = AddrType::ipv6;
        } else {
            return tl::unexpected("origin: invalid address type");
        }
    } else {
        return tl::unexpected("origin: failed to parse address type");
    }

    // Address
    if (const auto address = parser.split(' ')) {
        o.unicast_address = *address;
    } else {
        return tl::unexpected("origin: failed to parse address");
    }

    return o;
}

std::string rav::sdp::to_string(const OriginField& field) {
    return fmt::format(
        "o={} {} {} {} {} {}", field.username.empty() ? "-" : field.username, field.session_id, field.session_version,
        to_string(field.network_type), to_string(field.address_type), field.unicast_address
    );
}

tl::expected<void, std::string> rav::sdp::validate(const OriginField& field) {
    if (field.session_id.empty()) {
        return tl::unexpected("origin: session id is empty");
    }

    if (field.unicast_address.empty()) {
        return tl::unexpected("origin: unicast address is empty");
    }

    if (field.network_type == NetwType::undefined) {
        return tl::unexpected("origin: network type is undefined");
    }

    if (field.address_type == AddrType::undefined) {
        return tl::unexpected("origin: address type is undefined");
    }

    return {};
}
