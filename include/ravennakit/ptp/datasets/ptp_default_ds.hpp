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
    uint8_t priority1 {128};  // Default for default profile
    uint8_t priority2 {128};  // Default for default profile
    uint8_t domain_number {0};
    bool slave_only {false }; // Default for default profile
    uint16_t sdo_id { 0 };  // 12 bit on the wire (0-4095), default for default profile
};

}  // namespace rav
