/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/sdp/detail/sdp_source_filter.hpp"

#include "ravennakit/core/string_parser.hpp"

rav::sdp::source_filter::parse_result<rav::sdp::source_filter>
rav::sdp::source_filter::parse_new(const std::string_view line) {
    source_filter filter;
    string_parser parser(line);

    // Skip leading space
    if (!parser.skip(' ') ) {
        return parse_result<source_filter>::err("source_filter: leading space not found");
    }

    // Filter mode
    const auto filter_mode = parser.split(' ');
    if (!filter_mode) {
        return parse_result<source_filter>::err("source_filter: filter mode not found");
    }
    if (filter_mode == "incl") {
        filter.mode_ = filter_mode::include;
    } else if (filter_mode == "excl") {
        filter.mode_ = filter_mode::exclude;
    } else {
        return parse_result<source_filter>::err("source_filter: invalid filter mode");
    }

    // Network type
    const auto netw_type = parser.split(' ');
    if (!netw_type) {
        return parse_result<source_filter>::err("source_filter: network type not found");
    }

    if (netw_type == k_sdp_inet) {
        filter.net_type_ = netw_type::internet;
    } else {
        return parse_result<source_filter>::err("source_filter: invalid network type");
    }

    // Address type
    const auto addr_type = parser.split(' ');
    if (!addr_type) {
        return parse_result<source_filter>::err("source_filter: address type not found");
    }

    if (addr_type == k_sdp_ipv4) {
        filter.addr_type_ = addr_type::ipv4;
    } else if (addr_type == k_sdp_ipv6) {
        filter.addr_type_ = addr_type::ipv6;
    } else if (addr_type == k_sdp_wildcard) {
        filter.addr_type_ = addr_type::both;
    } else {
        return parse_result<source_filter>::err("source_filter: invalid address type");
    }

    // Destination address
    const auto dest_address = parser.split(' ');
    if (!dest_address) {
        return parse_result<source_filter>::err("source_filter: destination address not found");
    }

    if (dest_address->empty()) {
        return parse_result<source_filter>::err("source_filter: destination address is empty");
    }

    filter.dest_address_ = *dest_address;

    for (auto i = 0; i < RAV_LOOP_UPPER_BOUND; ++i) {
        const auto src_address = parser.split(' ');
        if (!src_address) {
            break;
        }
        if (src_address->empty()) {
            return parse_result<source_filter>::err("source_filter: source address is empty");
        }
        filter.src_list_.emplace_back(*src_address);
    }

    return parse_result<source_filter>::ok(filter);
}
