/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_connection_info.hpp"

#include "ravennakit/core/string_parser.hpp"
#include "ravennakit/sdp/detail/sdp_constants.hpp"

tl::expected<void, std::string> rav::sdp::ConnectionInfoField::validate() const {
    if (network_type == NetwType::undefined) {
        return tl::unexpected("connection: network type is undefined");
    }
    if (address_type == AddrType::undefined) {
        return tl::unexpected("connection: address type is undefined");
    }
    if (address.empty()) {
        return tl::unexpected("connection: address is empty");
    }
    if (address_type == AddrType::ipv4) {
        if (!ttl.has_value()) {
            return tl::unexpected("connection: ttl is required for ipv4 address");
        }
    } else if (address_type == AddrType::ipv6) {
        if (ttl.has_value()) {
            return tl::unexpected("connection: ttl is not allowed for ipv6 address");
        }
    }
    return {};
}

tl::expected<std::string, std::string> rav::sdp::ConnectionInfoField::to_string() const {
    auto result = validate();
    if (!validate()) {
        return tl::unexpected(result.error());
    }
    return fmt::format(
        "c={} {} {}{}{}", sdp::to_string(network_type), sdp::to_string(address_type), address,
        ttl.has_value() ? "/" + std::to_string(ttl.value()) : "",
        number_of_addresses.has_value() ? "/" + std::to_string(number_of_addresses.value()) : ""
    );
}

rav::sdp::ConnectionInfoField::parse_result<rav::sdp::ConnectionInfoField>
rav::sdp::ConnectionInfoField::parse_new(const std::string_view line) {
    string_parser parser(line);

    if (!parser.skip("c=")) {
        return parse_result<ConnectionInfoField>::err("connection: expecting 'c='");
    }

    ConnectionInfoField info;

    // Network type
    if (const auto network_type = parser.split(' ')) {
        if (*network_type == sdp::k_sdp_inet) {
            info.network_type = sdp::NetwType::internet;
        } else {
            return parse_result<ConnectionInfoField>::err("connection: invalid network type");
        }
    } else {
        return parse_result<ConnectionInfoField>::err("connection: failed to parse network type");
    }

    // Address type
    if (const auto address_type = parser.split(' ')) {
        if (*address_type == sdp::k_sdp_ipv4) {
            info.address_type = sdp::AddrType::ipv4;
        } else if (*address_type == sdp::k_sdp_ipv6) {
            info.address_type = sdp::AddrType::ipv6;
        } else {
            return parse_result<ConnectionInfoField>::err("connection: invalid address type");
        }
    } else {
        return parse_result<ConnectionInfoField>::err("connection: failed to parse address type");
    }

    // Address
    if (const auto address = parser.split('/')) {
        info.address = *address;
    }

    if (parser.exhausted()) {
        return parse_result<ConnectionInfoField>::ok(std::move(info));
    }

    // Parse optional ttl and number of addresses
    if (info.address_type == sdp::AddrType::ipv4) {
        if (auto ttl = parser.read_int<int32_t>()) {
            info.ttl = *ttl;
        } else {
            return parse_result<ConnectionInfoField>::err("connection: failed to parse ttl for ipv4 address");
        }
        if (parser.skip('/')) {
            if (auto num_addresses = parser.read_int<int32_t>()) {
                info.number_of_addresses = *num_addresses;
            } else {
                return parse_result<ConnectionInfoField>::err(
                    "connection: failed to parse number of addresses for ipv4 address"
                );
            }
        }
    } else if (info.address_type == sdp::AddrType::ipv6) {
        if (auto num_addresses = parser.read_int<int32_t>()) {
            info.number_of_addresses = *num_addresses;
        } else {
            return parse_result<ConnectionInfoField>::err(
                "connection: failed to parse number of addresses for ipv4 address"
            );
        }
    }

    if (!parser.exhausted()) {
        return parse_result<ConnectionInfoField>::err("connection: unexpected characters at end of line");
    }

    return parse_result<ConnectionInfoField>::ok(std::move(info));
}
