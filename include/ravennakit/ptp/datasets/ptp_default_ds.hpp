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

#include "../ptp_types.hpp"

namespace rav {

struct ptp_default_ds {
    // Static members
    ptp_clock_identity clock_identity;
    uint16_t number_ports {};
    // Dynamic members
    ptp_clock_quality clock_quality;
    // Configurable members
    uint8_t priority1 {};
    uint8_t priority2 {};
    uint8_t domain_number {};
    bool slave_only {};
    uint16_t sdo_id {};  // 12 bit on the wire (0-4095)
};

}  // namespace rav
