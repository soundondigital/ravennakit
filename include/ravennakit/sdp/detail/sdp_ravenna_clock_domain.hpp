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

#include "ravennakit/core/expected.hpp"

#include <string_view>

namespace rav::sdp {

/**
 * Defines a clock source and domain. This is a RAVENNA-specific attribute extension to the SDP specification.
 */
struct RavennaClockDomain {
    static constexpr auto k_attribute_name = "clock-domain";
    enum class SyncSource { undefined, ptp_v2 };

    SyncSource source {SyncSource::undefined};
    int32_t domain {};

    /**
     * Validates the values of this structure.
     * @returns An error message if the values are invalid.
     */
    [[nodiscard]] tl::expected<void, std::string> validate() const;

    /**
     * @return The string representation of this structure.
     */
    [[nodiscard]] tl::expected<std::string, std::string> to_string() const;

    /**
     * @param source The sync source to convert.
     * @returns A string representation of the sync source.
     */
    static std::string to_string(SyncSource source);

    /**
     * Parses a new instance of this structure from a string.
     * @param line The string to parse.
     * @return A parse result.
     */
    static tl::expected<RavennaClockDomain, std::string> parse_new(std::string_view line);
};

}  // namespace rav::sdp
