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

#include "ravennakit/core/log.hpp"
#include "ravennakit/core/util.hpp"
#include "ravennakit/core/net/interfaces/mac_address.hpp"
#include "ravennakit/core/util/todo.hpp"
#include "ravennakit/core/containers/buffer_view.hpp"
#include "ravennakit/core/containers/byte_buffer.hpp"
#include "ravennakit/core/streams/output_stream.hpp"

#include <cstdint>

namespace rav {

/**
 * Represents a PTP clock identity.
 * IEEE1588-2019: 5.3.4
 */
struct ptp_clock_identity {
    std::array<uint8_t, 8> data {};

    /**
     * Construct a PTP clock identity from a byte array.
     * @param mac_address The MAC address to construct the clock identity from.
     */
    static ptp_clock_identity from_mac_address(const mac_address& mac_address) {
        const auto mac_bytes = mac_address.bytes();
        return {mac_bytes[0], mac_bytes[1], mac_bytes[2], 0xff, 0xfe, mac_bytes[3], mac_bytes[4], mac_bytes[5]};
    }

    /**
     * Construct a PTP clock identity from a byte array.
     * @param view The data view to construct the clock identity from. Must be at least 8 bytes long.
     */
    static ptp_clock_identity from_data(buffer_view<const uint8_t> view) {
        RAV_ASSERT(view.size() >= 8, "Data is too short to construct a PTP clock identity");
        ptp_clock_identity clock_identity;
        std::memcpy(clock_identity.data.data(), view.data(), sizeof(clock_identity.data));
        return clock_identity;
    }

    /**
     * Write the ptp_announce_message to a byte buffer.
     * @param buffer The buffer to write to.
     */
    void write_to(byte_buffer& buffer) const {
        buffer.write(data.data(), data.size());
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
     * Check if the clock identity appears to be valid. This is not a formal validation, just a simple check. Logs
     * detailed messages if invalid.
     * Note: this is a partial implementation.
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
     * Checks the internal state of this object according to IEEE1588-2019. Asserts when something is wrong.
     */
    void assert_valid_state() const {
        RAV_ASSERT(!empty(), "All bytes are zero");
        RAV_ASSERT(data[0] != 0 || data[1] != 0 || data[2] != 0, "Bytes 0-2 are zero");
        RAV_ASSERT(data[3] == 0xff && data[4] == 0xfe, "Bytes 3-4 are not 0xfffe");
        RAV_ASSERT(data[5] != 0 || data[6] != 0 || data[7] != 0, "Bytes 5-7 are zero");
    }

    /**
     * @return True if all bytes are zero, false otherwise.
     */
    [[nodiscard]] bool empty() const {
        return std::all_of(data.data(), data.data() + 8, [](const uint8_t byte) {
            return byte == 0;
        });
    }

    friend bool operator==(const ptp_clock_identity& lhs, const ptp_clock_identity& rhs) {
        return lhs.data == rhs.data;
    }

    friend bool operator!=(const ptp_clock_identity& lhs, const ptp_clock_identity& rhs) {
        return lhs.data != rhs.data;
    }

    friend bool operator<(const ptp_clock_identity& lhs, const ptp_clock_identity& rhs) {
        return std::lexicographical_compare(lhs.data.begin(), lhs.data.end(), rhs.data.begin(), rhs.data.end());
    }

    friend bool operator>(const ptp_clock_identity& lhs, const ptp_clock_identity& rhs) {
        return std::lexicographical_compare(rhs.data.begin(), rhs.data.end(), lhs.data.begin(), lhs.data.end());
    }
};

}  // namespace rav
