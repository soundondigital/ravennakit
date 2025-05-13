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

#include <string>
#include <fmt/format.h>

namespace rav::nmos {

/**
 * Represents the version of the NMOS API. Not to be confused with the version of resources.
 */
struct ApiVersion {
    int16_t major {0};
    int16_t minor {0};

    /**
     * @return  True if the version is valid, false otherwise.
     */
    [[nodiscard]] bool is_valid() const {
        return major > 0 && minor >= 0;
    }

    /**
     * @return A string representation of the version in the format "vX.Y".
     */
    [[nodiscard]] std::string to_string() const {
        return fmt::format("v{}.{}", major, minor);
    }

    /**
     * Creates an ApiVersion from a string.
     * @return An optional ApiVersion. If the string is not valid, an empty optional is returned.
     */
    static std::optional<ApiVersion> from_string(const std::string_view str) {
        StringParser parser(str);

        if (!parser.skip('v')) {
            return std::nullopt;
        }

        const auto major = parser.read_int<int16_t>();
        if (!major) {
            return std::nullopt;
        }

        if (!parser.skip('.')) {
            return std::nullopt;
        }

        const auto minor = parser.read_int<int16_t>();
        if (!minor) {
            return std::nullopt;
        }

        if (!parser.exhausted()) {
            return std::nullopt;
        }

        return ApiVersion {*major, *minor};
    }

    static constexpr ApiVersion v1_2() {
        return {1, 2};
    }

    static constexpr ApiVersion v1_3() {
        return {1, 3};
    }

    friend bool operator==(const ApiVersion& lhs, const ApiVersion& rhs) {
        return lhs.major == rhs.major && lhs.minor == rhs.minor;
    }

    friend bool operator!=(const ApiVersion& lhs, const ApiVersion& rhs) {
        return !(lhs == rhs);
    }
};

}  // namespace rav::nmos
