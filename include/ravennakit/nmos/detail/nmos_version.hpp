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
#include "ravennakit/ptp/types/ptp_timestamp.hpp"

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

    Version() = default;

    Version(const uint64_t seconds_, const uint32_t nanoseconds_) : seconds(seconds_), nanoseconds(nanoseconds_) {
        RAV_ASSERT(nanoseconds < 1000000000, "Nanoseconds must be less than 1 billion.");
    }

    explicit Version(const ptp::Timestamp timestamp) :
        seconds(timestamp.raw_seconds()), nanoseconds(timestamp.raw_nanoseconds()) {}

    friend bool operator<(const Version& lhs, const Version& rhs) {
        return lhs.seconds < rhs.seconds || (lhs.seconds == rhs.seconds && lhs.nanoseconds < rhs.nanoseconds);
    }

    friend bool operator<=(const Version& lhs, const Version& rhs) {
        return rhs >= lhs;
    }

    friend bool operator>(const Version& lhs, const Version& rhs) {
        return rhs < lhs;
    }

    friend bool operator>=(const Version& lhs, const Version& rhs) {
        return !(lhs < rhs);
    }

    /**
     * Increases the version by one nanosecond.
     */
    void inc() {
        if (nanoseconds < 999999999) {
            ++nanoseconds;
        } else {
            nanoseconds = 0;
            ++seconds;
        }
    }

    /**
     * Updates the version with a new timestamp.
     * If the new timestamp is greater than the current version, it updates the version.
     * Otherwise, it increments the version by one nanosecond.
     * @param timestamp The new timestamp to update the version with.
     */
    void update(const ptp::Timestamp timestamp) {
        if (timestamp > ptp::Timestamp(seconds, nanoseconds)) {
            seconds = timestamp.raw_seconds();
            nanoseconds = timestamp.raw_nanoseconds();
        } else {
            inc();
        }
    }

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
    [[nodiscard]] std::string to_string() const {
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
        if (!parser.skip(':')) {
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
