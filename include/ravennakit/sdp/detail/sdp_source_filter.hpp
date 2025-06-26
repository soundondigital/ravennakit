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

#include "sdp_types.hpp"
#include "ravennakit/core/expected.hpp"

#include <string>
#include <vector>

namespace rav::sdp {

class SourceFilter {
  public:
    static constexpr auto k_attribute_name = "source-filter";

    FilterMode mode {FilterMode::undefined};
    NetwType net_type {NetwType::undefined};
    AddrType addr_type {AddrType::undefined};
    std::string dest_address;  // Must correspond to the address of a connection info field.
    std::vector<std::string> src_list;
};

/**
 * Converts the source filter to a string.
 * @returns The source filter as a string.
 */
[[nodiscard]] std::string to_string(const SourceFilter& filter);

/**
 * Parses a connection info field from a string.
 * @param line The string to parse.
 * @return A pair containing the parse result and the connection info. When parsing fails, the connection info
 * will be a default-constructed object.
 */
tl::expected<SourceFilter, std::string> parse_source_filter(std::string_view line);

/**
 * Validates the source filter.
 * @return An error message if the source filter is invalid.
 */
[[nodiscard]] tl::expected<void, std::string> validate(const SourceFilter& filter);

}  // namespace rav::sdp
