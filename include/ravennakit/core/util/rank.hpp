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

namespace rav {

/**
 * Class to represent a rank.
 * The rank is a number, starting from zero, where the higher the number the lower the rank.
 */
class Rank {
  public:
    Rank() = default;
    ~Rank() = default;

    explicit Rank(const uint8_t rank) : rank_(rank) {}

    Rank(const Rank&) = default;
    Rank& operator=(const Rank&) = default;

    Rank(Rank&&) = default;
    Rank& operator=(Rank&&) = default;

    friend bool operator==(const Rank& lhs, const Rank& rhs) {
        return lhs.rank_ == rhs.rank_;
    }

    friend bool operator!=(const Rank& lhs, const Rank& rhs) {
        return !(lhs == rhs);
    }

    /**
     * @returns The rank value.
     */
    explicit operator uint8_t() const {
        return rank_;
    }

    /**
     * @return The rank value.
     */
    uint8_t value() const {
        return rank_;
    }

    /**
     * @return The rank value as an ordinal string (1st, 2nd, etc.) up until 10th.
     */
    const char* to_ordinal() const {
        switch (rank_) {
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
    const char* to_ordinal_latin(const bool short_form = false) const {
        switch (rank_) {
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
                return "N/A";
        }
    }

  private:
    uint8_t rank_ {};
};

}  // namespace rav
