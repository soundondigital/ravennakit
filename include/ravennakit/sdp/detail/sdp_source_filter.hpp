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
#include "ravennakit/core/result.hpp"

#include <string>
#include <vector>
#include <tl/expected.hpp>

namespace rav::sdp {

class source_filter {
  public:
    static constexpr auto k_attribute_name = "source-filter";

    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, std::string>;

    source_filter() = default;
    source_filter(
        filter_mode mode, netw_type net_type, addr_type addr_type, std::string dest_address,
        std::vector<std::string> src_list
    );

    /**
     * @returns The filter mode.
     */
    [[nodiscard]] filter_mode mode() const {
        return mode_;
    }

    /**
     * @returns The network type.
     */
    [[nodiscard]] netw_type network_type() const {
        return net_type_;
    }

    /**
     * @returns The address type.
     */
    [[nodiscard]] addr_type address_type() const {
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
    static parse_result<source_filter> parse_new(std::string_view line);

  private:
    filter_mode mode_ {filter_mode::undefined};
    netw_type net_type_ {netw_type::undefined};
    addr_type addr_type_ {addr_type::undefined};
    std::string dest_address_;  // Must correspond to the address of a connection info field.
    std::vector<std::string> src_list_;
};

}  // namespace rav::sdp
