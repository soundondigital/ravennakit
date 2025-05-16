/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/core/string_parser.hpp"

#include <cstdint>
#include <string>
#include <fmt/format.h>

namespace rav::nmos {

/**
 * Represents a version in the format of seconds and nanoseconds.
 *
 * The Version class encapsulates the concept of a specific point in time,
 * allowing for comparisons and operations such as version validity and ordering.
 */
struct Version {
    uint64_t seconds = 0;      // seconds since epoch
    uint32_t nanoseconds = 0;  // nanoseconds since the last second

    /**
     * Checks whether the NMOS resource version is valid.
     * A version is considered valid if either the `seconds` or `nanoseconds` component is non-zero.
     * @return true if the version is valid, false otherwise.
     */
    [[nodiscard]] bool is_valid() const {
        return seconds != 0 || nanoseconds != 0;
    }

    /**
     * @return A string representation of the version in the format "seconds.nanoseconds".
     */
    std::string to_string() const {
        return fmt::format("{}:{}", seconds, nanoseconds);
    }

    /**
     * Parses a given string to a Version object.
     * @param input The string to be parsed into an object.
     * @return An optional Version object if parsing is successful, otherwise std::nullopt.
     */
    static std::optional<Version> from_string(const std::string_view input) {
        StringParser parser(input);
        if (parser.skip(' ')) {
            return std::nullopt;
        }
        const auto seconds = parser.read_int<uint64_t>();
        if (!seconds) {
            return std::nullopt;
        }
        if (!parser.skip('.')) {
            return std::nullopt;
        }
        const auto nanoseconds = parser.read_int<uint32_t>();
        if (!nanoseconds) {
            return std::nullopt;
        }
        if (!parser.exhausted()) {
            return std::nullopt;
        }
        return Version {*seconds, *nanoseconds};
    }
};

}  // namespace rav::nmos
