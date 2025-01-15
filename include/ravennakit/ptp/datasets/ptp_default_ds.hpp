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

#include "ravennakit/ptp/types/ptp_clock_identity.hpp"
#include "ravennakit/ptp/types/ptp_clock_quality.hpp"
#include "ravennakit/ptp/types/ptp_sdo_id.hpp"

namespace rav {

/**
 * Represents the default data set as described in IEEE1588-2019: 8.2.1.
 */
struct ptp_default_ds {
    // Static members
    ptp_clock_identity clock_identity;
    uint16_t number_ports {};
    // Dynamic members
    ptp_clock_quality clock_quality;
    // Configurable members
    uint8_t priority1 {128};  // Default for default profile
    uint8_t priority2 {128};  // Default for default profile

    /// Domain number
    /// Default profile: 0
    uint8_t domain_number {0};

    /// Slave only
    /// Default profile: false (if configurable)
    bool slave_only {false};  // Default for default profile

    ptp_sdo_id sdo_id;  // 12 bit on the wire (0-4095), default for default profile

    explicit ptp_default_ds(const bool slave_only_) {
        slave_only = slave_only_;
        clock_quality = ptp_clock_quality(slave_only);
    }
};

}  // namespace rav
