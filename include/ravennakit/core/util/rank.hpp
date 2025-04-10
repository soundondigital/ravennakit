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

namespace rav {

inline const char* rank_to_ordinal(const size_t rank) {
    switch (rank) {
        case 0:
            return "0th";
        case 1:
            return "1st";
        case 2:
            return "2nd";
        case 3:
            return "3rd";
        case 4:
            return "4th";
        case 5:
            return "5th";
        case 6:
            return "6th";
        case 7:
            return "7th";
        case 8:
            return "8th";
        case 9:
            return "9th";
        case 10:
            return "10th";
        default:
            return "N/A";
    }
}

inline const char* rank_to_ordinal_latin(const size_t rank) {
    switch (rank) {
        case 0:
            return "0";
        case 1:
            return "primary";
        case 2:
            return "secondary";
        case 3:
            return "tertiary";
        case 4:
            return "quaternary";
        case 5:
            return "quinary";
        case 6:
            return "senary";
        case 7:
            return "septenary";
        case 8:
            return "octonary";
        case 9:
            return "nonary";
        case 10:
            return "denary";
        default:
            return "N/A";
    }
}

}  // namespace rav
