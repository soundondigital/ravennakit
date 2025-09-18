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

namespace rav::ptp {

/**
 * Represents a PTP clock identity.
 * IEEE1588-2019: 5.3.4
 */
struct ClockIdentity {
    std::array<uint8_t, 8> data {};

    /// Octet 6 & 7 of a ClockIdentity for when constructing from an EUI-48 based on IEEE1588-2019 7.5.2.2.2.2
    static constexpr uint8_t k_implementer_specific_octets[] = {0x2f, 0xaa};  // Random

    /**
     * Construct a PTP clock identity from a mac address based on IEEE1588-2019 7.5.2.2.2.2
     * @param mac_address The MAC address to construct the clock identity from.
     * @return A ClockIdentity or a std::nullopt when a ClockIdentity could not be constructed.
     */
    static std::optional<ClockIdentity> from_mac_address(const MacAddress& mac_address) {
        const auto mac_bytes = mac_address.bytes();
        if (std::all_of(mac_bytes.begin(), mac_bytes.end(), [&](const auto byte) {
                return byte == 0;
            })) {
            return std::nullopt;
        }
        return ClockIdentity {
            mac_bytes[0],
            mac_bytes[1],
            mac_bytes[2],
            mac_bytes[3],
            mac_bytes[4],
            mac_bytes[5],
            k_implementer_specific_octets[0],
            k_implementer_specific_octets[1]
        };
    }

    /**
     * Construct a PTP clock identity from a byte array.
     * @param view The data view to construct the clock identity from. Must be at least 8 bytes long.
     */
    static ClockIdentity from_data(BufferView<const uint8_t> view) {
        RAV_ASSERT(view.size() >= 8, "Data is too short to construct a PTP clock identity");
        ClockIdentity clock_identity;
        std::memcpy(clock_identity.data.data(), view.data(), sizeof(clock_identity.data));
        return clock_identity;
    }

    /**
     * Write the ptp_announce_message to a byte buffer.
     * @param buffer The buffer to write to.
     */
    void write_to(ByteBuffer& buffer) const {
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
     * Check if the clock identity appears to be valid. This is not a formal validation, just a simple check.
     * @return True if the clock identity appears to be valid, false otherwise.
     */
    [[nodiscard]] bool is_valid() const {
        if (all_zero()) {
            return false;
        }
        if (data[6] == k_implementer_specific_octets[0] && data[7] == k_implementer_specific_octets[1]) {
            return data[0] != 0 || data[1] != 0 || data[2] != 0 || data[3] != 0 || data[4] != 0 || data[5] != 0;
        }

        return true;
    }

    /**
     * Checks the internal state of this object according to IEEE1588-2019. Asserts when something is wrong.
     */
    void assert_valid_state() const {
        RAV_ASSERT(!all_zero(), "All bytes are zero");
    }

    /**
     * @return True if all bytes are zero, false otherwise.
     */
    [[nodiscard]] bool all_zero() const {
        return std::all_of(data.begin(), data.end(), [](const uint8_t byte) {
            return byte == 0;
        });
    }

    friend bool operator==(const ClockIdentity& lhs, const ClockIdentity& rhs) {
        return lhs.data == rhs.data;
    }

    friend bool operator!=(const ClockIdentity& lhs, const ClockIdentity& rhs) {
        return lhs.data != rhs.data;
    }

    friend bool operator<(const ClockIdentity& lhs, const ClockIdentity& rhs) {
        return std::lexicographical_compare(lhs.data.begin(), lhs.data.end(), rhs.data.begin(), rhs.data.end());
    }

    friend bool operator>(const ClockIdentity& lhs, const ClockIdentity& rhs) {
        return std::lexicographical_compare(rhs.data.begin(), rhs.data.end(), lhs.data.begin(), lhs.data.end());
    }
};

}  // namespace rav::ptp
