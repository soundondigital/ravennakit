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

rav::sdp::SourceFilter::SourceFilter(
    FilterMode mode, NetwType net_type, AddrType addr_type, std::string dest_address,
    std::vector<std::string> src_list
) :
    mode_(mode),
    net_type_(net_type),
    addr_type_(addr_type),
    dest_address_(std::move(dest_address)),
    src_list_(std::move(src_list)) {}

tl::expected<std::string, std::string> rav::sdp::SourceFilter::to_string() const {
    auto validated = validate();
    if (!validated) {
        return tl::unexpected(validated.error());
    }
    auto txt = fmt::format(
        "a={}: {} {} {} {}", k_attribute_name, sdp::to_string(mode_), sdp::to_string(net_type_),
        sdp::to_string(addr_type_), dest_address_
    );
    for (const auto& src : src_list_) {
        fmt::format_to(std::back_inserter(txt), " {}", src);
    }
    return txt;
}

tl::expected<void, std::string> rav::sdp::SourceFilter::validate() const {
    if (FilterMode::undefined == mode_) {
        return tl::unexpected("source_filter: mode is undefined");
    }
    if (NetwType::undefined == net_type_) {
        return tl::unexpected("source_filter: network type is undefined");
    }
    if (AddrType::undefined == addr_type_) {
        return tl::unexpected("source_filter: address type is undefined");
    }
    if (dest_address_.empty()) {
        return tl::unexpected("source_filter: destination address is empty");
    }
    if (src_list_.empty()) {
        return tl::unexpected("source_filter: source list is empty");
    }
    return {};
}

tl::expected<rav::sdp::SourceFilter, std::string> rav::sdp::SourceFilter::parse_new(const std::string_view line) {
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
        filter.mode_ = FilterMode::include;
    } else if (filter_mode == "excl") {
        filter.mode_ = FilterMode::exclude;
    } else {
        return tl::unexpected("source_filter: invalid filter mode");
    }

    // Network type
    const auto netw_type = parser.split(' ');
    if (!netw_type) {
        return tl::unexpected("source_filter: network type not found");
    }

    if (netw_type == k_sdp_inet) {
        filter.net_type_ = NetwType::internet;
    } else {
        return tl::unexpected("source_filter: invalid network type");
    }

    // Address type
    const auto addr_type = parser.split(' ');
    if (!addr_type) {
        return tl::unexpected("source_filter: address type not found");
    }

    if (addr_type == k_sdp_ipv4) {
        filter.addr_type_ = AddrType::ipv4;
    } else if (addr_type == k_sdp_ipv6) {
        filter.addr_type_ = AddrType::ipv6;
    } else if (addr_type == k_sdp_wildcard) {
        filter.addr_type_ = AddrType::both;
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

    filter.dest_address_ = *dest_address;

    for (auto i = 0; i < RAV_LOOP_UPPER_BOUND; ++i) {
        const auto src_address = parser.split(' ');
        if (!src_address) {
            break;
        }
        if (src_address->empty()) {
            return tl::unexpected("source_filter: source address is empty");
        }
        filter.src_list_.emplace_back(*src_address);
    }

    return filter;
}
