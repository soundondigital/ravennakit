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

    SourceFilter() = default;
    SourceFilter(
        FilterMode mode, NetwType net_type, AddrType addr_type, std::string dest_address,
        std::vector<std::string> src_list
    );

    /**
     * @returns The filter mode.
     */
    [[nodiscard]] FilterMode mode() const {
        return mode_;
    }

    /**
     * @returns The network type.
     */
    [[nodiscard]] NetwType network_type() const {
        return net_type_;
    }

    /**
     * @returns The address type.
     */
    [[nodiscard]] AddrType address_type() const {
        return addr_type_;
    }

    /**
     * @returns The destination address.
     */
    [[nodiscard]] const std::string& dest_address() const {
        return dest_address_;
    }

    /**
     * @returns The list of source addresses.
     */
    [[nodiscard]] const std::vector<std::string>& src_list() const {
        return src_list_;
    }

    /**
     * Converts the source filter to a string.
     * @returns The source filter as a string.
     */
    [[nodiscard]] tl::expected<std::string, std::string> to_string() const;

    /**
     * Validates the source filter.
     * @return An error message if the source filter is invalid.
     */
    [[nodiscard]] tl::expected<void, std::string> validate() const;

    /**
     * Parses a connection info field from a string.
     * @param line The string to parse.
     * @return A pair containing the parse result and the connection info. When parsing fails, the connection info
     * will be a default-constructed object.
     */
    static tl::expected<SourceFilter, std::string> parse_new(std::string_view line);

  private:
    FilterMode mode_ {FilterMode::undefined};
    NetwType net_type_ {NetwType::undefined};
    AddrType addr_type_ {AddrType::undefined};
    std::string dest_address_;  // Must correspond to the address of a connection info field.
    std::vector<std::string> src_list_;
};

}  // namespace rav::sdp
