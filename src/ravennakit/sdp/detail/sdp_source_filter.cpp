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
#include "ravennakit/sdp/detail/sdp_constants.hpp"

tl::expected<rav::sdp::SourceFilter, std::string> rav::sdp::parse_source_filter(const std::string_view line) {
    SourceFilter filter;
    StringParser parser(line);

    // Skip leading space
    if (!parser.skip(' ')) {
        return tl::unexpected("source_filter: leading space not found");
    }

    // Filter mode
    const auto filter_mode = parser.split(' ');
    if (!filter_mode) {
        return tl::unexpected("source_filter: filter mode not found");
    }
    if (filter_mode == "incl") {
        filter.mode = FilterMode::include;
    } else if (filter_mode == "excl") {
        filter.mode = FilterMode::exclude;
    } else {
        return tl::unexpected("source_filter: invalid filter mode");
    }

    // Network type
    const auto netw_type = parser.split(' ');
    if (!netw_type) {
        return tl::unexpected("source_filter: network type not found");
    }

    if (netw_type == k_sdp_inet) {
        filter.net_type = NetwType::internet;
    } else {
        return tl::unexpected("source_filter: invalid network type");
    }

    // Address type
    const auto addr_type = parser.split(' ');
    if (!addr_type) {
        return tl::unexpected("source_filter: address type not found");
    }

    if (addr_type == k_sdp_ipv4) {
        filter.addr_type = AddrType::ipv4;
    } else if (addr_type == k_sdp_ipv6) {
        filter.addr_type = AddrType::ipv6;
    } else if (addr_type == k_sdp_wildcard) {
        filter.addr_type = AddrType::both;
    } else {
        return tl::unexpected("source_filter: invalid address type");
    }

    // Destination address
    const auto dest_address = parser.split(' ');
    if (!dest_address) {
        return tl::unexpected("source_filter: destination address not found");
    }

    if (dest_address->empty()) {
        return tl::unexpected("source_filter: destination address is empty");
    }

    filter.dest_address = *dest_address;

    constexpr auto s_loop_upper_bound = 100'000;
    for (auto i = 0; i < s_loop_upper_bound; ++i) {
        const auto src_address = parser.split(' ');
        if (!src_address) {
            break;
        }
        if (src_address->empty()) {
            return tl::unexpected("source_filter: source address is empty");
        }
        filter.src_list.emplace_back(*src_address);
    }

    return filter;
}

std::string rav::sdp::to_string(const SourceFilter& filter) {
    auto txt = fmt::format(
        "a={}: {} {} {} {}", SourceFilter::k_attribute_name, to_string(filter.mode), to_string(filter.net_type),
        to_string(filter.addr_type), filter.dest_address
    );
    for (const auto& src : filter.src_list) {
        fmt::format_to(std::back_inserter(txt), " {}", src);
    }
    return txt;
}

tl::expected<void, std::string> rav::sdp::validate(const SourceFilter& filter) {
    if (FilterMode::undefined == filter.mode) {
        return tl::unexpected("source_filter: mode is undefined");
    }
    if (NetwType::undefined == filter.net_type) {
        return tl::unexpected("source_filter: network type is undefined");
    }
    if (AddrType::undefined == filter.addr_type) {
        return tl::unexpected("source_filter: address type is undefined");
    }
    if (filter.dest_address.empty()) {
        return tl::unexpected("source_filter: destination address is empty");
    }
    if (filter.src_list.empty()) {
        return tl::unexpected("source_filter: source list is empty");
    }
    return {};
}
