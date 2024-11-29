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

rav::sdp::origin_field::parse_result<rav::sdp::origin_field> rav::sdp::origin_field::parse_new(std::string_view line) {
    string_parser parser(line);

    if (!parser.skip("o=")) {
        return parse_result<origin_field>::err("origin: expecting 'o='");
    }

    origin_field o;

    // Username
    if (const auto username = parser.split(' ')) {
        o.username = *username;
    } else {
        return parse_result<origin_field>::err("origin: failed to parse username");
    }

    // Session id
    if (const auto session_id = parser.split(' ')) {
        o.session_id = *session_id;
    } else {
        return parse_result<origin_field>::err("origin: failed to parse session id");
    }

    // Session version
    if (const auto version = parser.read_int<int32_t>()) {
        o.session_version = *version;
        parser.skip(' ');
    } else {
        return parse_result<origin_field>::err("origin: failed to parse session version");
    }

    // Network type
    if (const auto network_type = parser.split(' ')) {
        if (*network_type != k_sdp_inet) {
            return parse_result<origin_field>::err("origin: invalid network type");
        }
        o.network_type = netw_type::internet;
    } else {
        return parse_result<origin_field>::err("origin: failed to parse network type");
    }

    // Address type
    if (const auto address_type = parser.split(' ')) {
        if (*address_type == k_sdp_ipv4) {
            o.address_type = addr_type::ipv4;
        } else if (*address_type == k_sdp_ipv6) {
            o.address_type = addr_type::ipv6;
        } else {
            return parse_result<origin_field>::err("origin: invalid address type");
        }
    } else {
        return parse_result<origin_field>::err("origin: failed to parse address type");
    }

    // Address
    if (const auto address = parser.split(' ')) {
        o.unicast_address = *address;
    } else {
        return parse_result<origin_field>::err("origin: failed to parse address");
    }

    return parse_result<origin_field>::ok(std::move(o));
}
