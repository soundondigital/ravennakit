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

#include "ravennakit/core/net/interfaces/mac_address.hpp"
#include "ravennakit/core/util/todo.hpp"

#include <cstdint>

namespace rav {

/**
 * Represents a PTP clock identity.
 * IEEE1588-2019: 5.3.4
 */
struct ptp_clock_identity {
    uint8_t data[8] {};

    /**
     * Construct a PTP clock identity from a byte array.
     * @param mac_address The MAC address to construct the clock identity from.
     */
    static ptp_clock_identity from_mac_address(const mac_address& mac_address) {
        const auto mac_bytes = mac_address.bytes();
        return {mac_bytes[0], mac_bytes[1], mac_bytes[2], 0xff, 0xfe, mac_bytes[3], mac_bytes[4], mac_bytes[5]};
    }

    /**
     * @return A string representation of the clock identity.
     */
    [[nodiscard]] std::string to_string() const {
        return fmt::format(
            "{:02x}-{:02x}-{:02x}-{:02x}-{:02x}-{:02x}-{:02x}-{:02x}", data[0], data[1], data[2], data[3], data[4],
            data[5], data[6], data[7]
        );
    }

    /**
     * Check if the clock identity appears to be valid. This is not a formal validation, just a simple check.
     * @return True if the clock identity appears to be valid, false otherwise.
     */
    [[nodiscard]] bool is_valid() const {
        if (empty()) {
            return false;
        }
        if (data[0] == 0 && data[1] == 0 && data[2] == 0) {
            return false;
        }
        if (data[5] == 0 && data[6] == 0 && data[7] == 0) {
            return false;
        }
        // Not sure what valid values for bytes 3 and 4 are at the moment.
        return true;
    }

    /**
     * @return True if all bytes are zero, false otherwise.
     */
    [[nodiscard]] bool empty() const {
        return std::all_of(data, data + 8, [](const uint8_t byte) {
            return byte == 0;
        });
    }

    friend bool operator==(const ptp_clock_identity& lhs, const ptp_clock_identity& rhs) {
        return std::memcmp(lhs.data, rhs.data, sizeof(lhs.data)) == 0;
    }

    friend bool operator!=(const ptp_clock_identity& lhs, const ptp_clock_identity& rhs) {
        return !(lhs == rhs);
    }
};

}  // namespace rav
