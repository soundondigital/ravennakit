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

#include "ptp_definitions.hpp"
#include "ravennakit/core/types/uint48.hpp"
#include "ravennakit/core/format.hpp"
#include "ravennakit/core/net/interfaces/mac_address.hpp"
#include "ravennakit/core/util/todo.hpp"

#include <string>

namespace rav {

struct ptp_timestamp {
    uint48_t seconds;
    uint32_t nanoseconds {};
};

struct ptp_clock_identity {
    uint8_t data[8] {};

    static ptp_clock_identity from_mac_address(const mac_address& mac_address) {
        TODO("Implement");
    }

    [[nodiscard]] std::string to_string() const {
        return fmt::format(
            "{:02x}-{:02x}-{:02x}-{:02x}-{:02x}-{:02x}-{:02x}-{:02x}", data[0], data[1], data[2], data[3], data[4],
            data[5], data[6], data[7]
        );
    }

    friend bool operator==(const ptp_clock_identity& lhs, const ptp_clock_identity& rhs) {
        return std::memcmp(lhs.data, rhs.data, sizeof(lhs.data)) == 0;
    }

    friend bool operator!=(const ptp_clock_identity& lhs, const ptp_clock_identity& rhs) {
        return !(lhs == rhs);
    }
};

struct ptp_port_identity {
    ptp_clock_identity clock_identity;
    uint16_t port_number {};

    friend bool operator==(const ptp_port_identity& lhs, const ptp_port_identity& rhs) {
        return std::tie(lhs.clock_identity, lhs.port_number) == std::tie(rhs.clock_identity, rhs.port_number);
    }

    friend bool operator!=(const ptp_port_identity& lhs, const ptp_port_identity& rhs) {
        return !(lhs == rhs);
    }
};

/**
 * PTP Clock Quality
 * IEEE1588-2019 section 7.6.2.5, Table 4
 */
struct ptp_clock_quality {
    /// The clock class. Default is 248, for slave-only the value is 255.
    uint8_t clock_class {};
    ptp_clock_accuracy clock_accuracy {ptp_clock_accuracy::unknown};
    uint16_t offset_scaled_log_variance {};
};

}  // namespace rav
