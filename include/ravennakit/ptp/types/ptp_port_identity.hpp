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

namespace rav {

/**
 * Represents a PTP port identity.
 * IEEE1588-2019: 5.3.5
 */
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

}
