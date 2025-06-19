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

    /**
     * Parses a connection info field from a string.
     * @param line The string to parse.
     * @return A result object containing either the newly parsed value, or an error string.
     */
    static tl::expected<ConnectionInfoField, std::string> parse_new(std::string_view line);
};

}
