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

namespace rav::sdp {

/**
 * A type which represents the origin field (o=*) of an SDP session description.
 * In general, the origin serves as a globally unique identifier for this version of the session description, and
 * the subfields excepting the version, taken together identify the session irrespective of any modifications.
 */
struct OriginField {
    /// The user's login on the originating host, or "-" if the originating host does not support the concept of
    /// user IDs.
    std::string username;

    /// Holds a numeric string such that the tuple of <username>, <sess-id>, <nettype>, <addrtype>, and
    /// <unicast-address> forms a globally unique identifier for the session.
    std::string session_id;

    /// The version number for this session description.
    int session_version {};

    /// Specifies the type of network.
    NetwType network_type {NetwType::undefined};

    /// Specifies the type of address.
    AddrType address_type {AddrType::undefined};

    /// The address of the machine from which the session was created.
    std::string unicast_address;
};

/**
 * Parses an origin field from a string.
 * @param line The string to parse.
 * @return A result indicating success or failure. When parsing fails, the error message will contain a
 * description of the error.
 */
[[nodiscard]] tl::expected<OriginField, std::string> parse_origin(std::string_view line);

/**
 * Converts the origin field to a string.
 */
[[nodiscard]] std::string to_string(const OriginField& field);

/**
 * Validates the members of this struct.
 * @return A result indicating success or failure. When validation fails, the error message will contain a
 * description of the error.
 */
[[nodiscard]] tl::expected<void, std::string> validate(const OriginField& field);

}  // namespace rav::sdp
