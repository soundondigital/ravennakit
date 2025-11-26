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

#include <cstdint>

namespace rav::rank {

static constexpr uint8_t primary = 0;
static constexpr uint8_t secondary = 1;

/**
 * @return The rank value as an ordinal string (1st, 2nd, etc.) up until 10th.
 */
[[nodiscard]] inline const char* to_ordinal(const uint8_t rank) {
    switch (rank) {
        case 0:
            return "1st";
        case 1:
            return "2nd";
        case 2:
            return "3rd";
        case 3:
            return "4th";
        case 4:
            return "5th";
        case 5:
            return "6th";
        case 6:
            return "7th";
        case 7:
            return "8th";
        case 8:
            return "9th";
        case 9:
            return "10th";
        default:
            return "";
    }
}

/**
 * @return The rank value as an ordinal string (primary, secondary, etc.) up until tenth (10th).
 */
[[nodiscard]] inline const char* to_ordinal_latin(const uint8_t rank, const bool short_form = false) {
    switch (rank) {
        case 0:
            return short_form ? "pri" : "primary";
        case 1:
            return short_form ? "sec" : "secondary";
        case 2:
            return "tertiary";
        case 3:
            return "fourth";
        case 4:
            return "fifth";
        case 5:
            return "sixth";
        case 6:
            return "seventh";
        case 7:
            return "eighth";
        case 8:
            return "ninth";
        case 9:
            return "tenth";
        case 10:
        default:
            return "";
    }
}

}  // namespace rav::rank
