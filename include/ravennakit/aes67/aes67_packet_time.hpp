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

#include "ravennakit/core/math/fraction.hpp"

#include <cstdint>
#include <cmath>
#include <tuple>

namespace rav::aes67 {

/**
 * Represents packet time as specified in AES67-2023 Section 7.2.
 */
class PacketTime {
  public:
    PacketTime() = default;

    PacketTime(const uint8_t numerator, const uint8_t denominator) : fraction_ {numerator, denominator} {}

    /**
     * @param sample_rate The sample rate of the audio.
     * @return The signaled packet time as used in SDP.
     */
    [[nodiscard]] float signaled_ptime(const uint32_t sample_rate) const {
        if (sample_rate % 48000 > 0) {
            return static_cast<float>(fraction_.numerator) * static_cast<float>(sample_rate / 48000 + 1)  // NOLINT
                * 48000 / static_cast<float>(sample_rate) / static_cast<float>(fraction_.denominator);
        }

        return static_cast<float>(fraction_.numerator) / static_cast<float>(fraction_.denominator);
    }

    /**
     * @param sample_rate The sample rate of the audio.
     * @return The number of frames in a packet.
     */
    [[nodiscard]] uint32_t framecount(const uint32_t sample_rate) const {
        return framecount(signaled_ptime(sample_rate), sample_rate);
    }

    /**
     * @returns True if the packet time is valid, false otherwise.
     */
    [[nodiscard]] bool is_valid() const {
        if (fraction_.denominator == 0) {
            return false;
        }
        if (fraction_.numerator == 0) {
            return false;
        }
        return true;
    }

    /**
     * Calculates the amount of frames for a given signaled packet time.
     * @param signaled_ptime The signaled packet time in milliseconds.
     * @param sample_rate The sample rate of the audio.
     * @return The number of frames in a packet.
     */
    static uint16_t framecount(const float signaled_ptime, const uint32_t sample_rate) {
        return static_cast<uint16_t>(std::round(signaled_ptime * static_cast<float>(sample_rate) / 1000.0f));
    }

    /**
     * @return A packet time of 125 microseconds.
     */
    static PacketTime us_125() {
        return PacketTime {1, 8};
    }

    /**
     * @return A packet time of 250 microseconds.
     */
    static PacketTime us_250() {
        return PacketTime {1, 4};
    }

    /**
     * @return A packet time of 333 microseconds.
     */
    static PacketTime us_333() {
        return PacketTime {1, 3};
    }

    /**
     * @return A packet time of 1 millisecond.
     */
    static PacketTime ms_1() {
        return PacketTime {1, 1};
    }

    /**
     * @return A packet time of 4 milliseconds.
     */
    static PacketTime ms_4() {
        return PacketTime {4, 1};
    }

    friend bool operator==(const PacketTime& lhs, const PacketTime& rhs) {
        return lhs.fraction_ == rhs.fraction_;
    }

    friend bool operator!=(const PacketTime& lhs, const PacketTime& rhs) {
        return !(lhs == rhs);
    }

  private:
    Fraction<uint8_t> fraction_ {};
};

}  // namespace rav::aes67
