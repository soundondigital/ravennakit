// SPDX-License-Identifier: AGPL-3.0-or-later
/*
 * Project: RAVENNAKIT (RAVENNA / AES67 / ST2110-30 SDK)
 * Copyright (c) 2024-2025 Sound on Digital
 *
 * This file is part of RAVENNAKIT.
 *
 * RAVENNAKIT is dual-licensed:
 *   1) Under the terms of the GNU Affero General Public License as published by
 *      the Free Software Foundation, either version 3 of the License, or
 *      (at your option) any later version (the "AGPL License"); and
 *   2) Under a commercial license from Sound on Digital, for customers who
 *      cannot (or do not wish to) comply with the AGPL License terms.
 *
 * If you obtained this file under the AGPL License, you may redistribute it
 * and/or modify it under the terms of the AGPL License. See the LICENSE
 * file in the project root for details.
 *
 * For commercial licensing, support, and other inquiries, please visit:
 *
 *     https://ravennakit.com
 *
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
    uint64_t session_version {};

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
