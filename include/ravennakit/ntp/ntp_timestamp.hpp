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

#include <cstdint>
#include <string>

namespace rav::ntp {

/**
 * Represents NTP wallclock time which is in seconds relative to 0h UTC on 1 January 1900. The full resolution NTP
 * timestamp is divided into an integer part (4 octets) and a fractional part (4 octets).
 */
class Timestamp {
  public:
    Timestamp() = default;

    /**
     * Constructs an NTP timestamp with separate integer and fraction parts.
     * @param integer The integer part of the timestamp.
     * @param fraction The fractional part of the timestamp.
     */
    Timestamp(const uint32_t integer, const uint32_t fraction) : integer_(integer), fraction_(fraction) {}

    /**
     * @returns The integer part of the timestamp.
     */
    [[nodiscard]] uint32_t integer() const {
        return integer_;
    }

    /**
     * @return The fractional part of the timestamp.
     */
    [[nodiscard]] uint32_t fraction() const {
        return fraction_;
    }

    /**
     * @return A string representation of the timestamp.
     */
    [[nodiscard]] std::string to_string() const {
        return std::to_string(integer_) + "." + std::to_string(fraction_);
    }

    /**
     * Generates a timestamp from a compact 32-bit integer representation. The compact representation consists of the
     * most significant 16 bits as the integer part and the least significant 16 bits representing the high-order bits
     * of the fractional part.
     * @param compact_encoded The compact encoded word.
     * @return The timestamp with compact resolution.
     */
    static Timestamp from_compact(const uint32_t compact_encoded) {
        return {(compact_encoded & 0xffff'0000) >> 16, compact_encoded << 16};
    }

    /**
     * Generates a timestamp from two uint16 values. The first uint16 represents the integer part, while the second
     * uint16 represents the high-order bits of the fractional part.
     * @param integer The compact integer.
     * @param fraction The compact fraction (highest bits).
     * @return The timestamp with compact resolution.
     */
    static Timestamp from_compact(const uint16_t integer, const uint16_t fraction) {
        return {integer, static_cast<uint32_t>(fraction << 16)};
    }

    friend bool operator==(const Timestamp& lhs, const Timestamp& rhs) {
        return lhs.integer_ == rhs.integer_ && lhs.fraction_ == rhs.fraction_;
    }

    friend bool operator!=(const Timestamp& lhs, const Timestamp& rhs) {
        return !(lhs == rhs);
    }

  private:
    uint32_t integer_ {};
    uint32_t fraction_ {};
};

}  // namespace rav::ntp
