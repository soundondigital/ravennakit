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

#include "ptp_clock_identity.hpp"
#include "ravennakit/core/byte_order.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/containers/byte_buffer.hpp"
#include "ravennakit/core/streams/output_stream.hpp"
#include "ravennakit/ptp/ptp_error.hpp"

#include <tl/expected.hpp>

namespace rav {

/**
 * Represents a PTP port identity.
 * IEEE1588-2019: 5.3.5
 */
struct ptp_port_identity {
    constexpr static uint16_t k_port_number_min = 0x1;     // Inclusive
    constexpr static uint16_t k_port_number_max = 0xfffe;  // Inclusive
    constexpr static uint16_t k_port_number_all = 0xffff;  // Means all ports

    ptp_clock_identity clock_identity;
    uint16_t port_number {};  // Valid range: [k_port_number_min, k_port_number_max]

    /**
     * Construct a PTP port identity from a byte array.
     * @param data The data to construct the port identity from. Must be at least 10 bytes long.
     */
    static tl::expected<ptp_port_identity, ptp_error> from_data(const buffer_view<const uint8_t> data) {
        if (data.size_bytes() < 10) {
            return tl::unexpected(ptp_error::invalid_message_length);
        }
        ptp_port_identity port_identity;
        port_identity.clock_identity = ptp_clock_identity::from_data(data);
        port_identity.port_number = rav::byte_order::read_be<uint16_t>(data.data() + 8);
        return port_identity;
    }

    /**
     * Write the ptp_announce_message to a byte buffer.
     * @param buffer The buffer to write to.
     */
    void write_to(byte_buffer& buffer) const {
        clock_identity.write_to(buffer);
        buffer.write_be(port_number);
    }

    /**
     * @return A string representation of the port identity.
     */
    [[nodiscard]] std::string to_string() const {
        return fmt::format("clock_identity={} port_number={}", clock_identity.to_string(), port_number);
    }

    /**
     * Checks the internal state of this object according to IEEE1588-2019. Asserts when something is wrong.
     */
    void assert_valid_state() const {
        clock_identity.assert_valid_state();
        RAV_ASSERT(port_number >= k_port_number_min, "port_number is below minimum");
        RAV_ASSERT(port_number <= k_port_number_max, "port_number is above maximum");
    }

    friend bool operator==(const ptp_port_identity& lhs, const ptp_port_identity& rhs) {
        return std::tie(lhs.clock_identity, lhs.port_number) == std::tie(rhs.clock_identity, rhs.port_number);
    }

    friend bool operator!=(const ptp_port_identity& lhs, const ptp_port_identity& rhs) {
        return !(lhs == rhs);
    }
};

}  // namespace rav
