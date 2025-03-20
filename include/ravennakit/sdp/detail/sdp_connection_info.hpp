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

#include <tl/expected.hpp>

namespace rav::sdp {

/**
 * A type representing the connection information (c=*) of an SDP session description.
 */
struct ConnectionInfoField {
    /// Specifies the type of network.
    NetwType network_type {NetwType::undefined};
    /// Specifies the type of address.
    AddrType address_type {AddrType::undefined};
    /// The address at which the media can be found.
    std::string address;
    /// Optional ttl
    std::optional<int32_t> ttl;
    /// Optional number of addresses
    std::optional<int32_t> number_of_addresses;

    /**
     * Validates the connection info.
     * @return An error message if the connection info is invalid.
     */
    [[nodiscard]] tl::expected<void, std::string> validate() const;

    /**
     * Converts the connection info to a string.
     * @return The connection info as a string.
     */
    [[nodiscard]] tl::expected<std::string, std::string> to_string() const;

    /// A type alias for a parse result.
    template<class T>
    using parse_result = result<T, std::string>;

    /**
     * Parses a connection info field from a string.
     * @param line The string to parse.
     * @return A pair containing the parse result and the connection info. When parsing fails, the connection info
     * will be a default-constructed object.
     */
    static parse_result<ConnectionInfoField> parse_new(std::string_view line);
};

}
