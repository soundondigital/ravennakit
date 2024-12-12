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

namespace rav {

struct ptp_timestamp {
    uint48_t seconds {};
    uint32_t nanoseconds {};
};

struct ptp_clock_identity {
    uint8_t data[8] {};
};

struct ptp_port_identity {
    ptp_clock_identity clock_identity;
    uint16_t port_number {};
};

/**
 * PTP Clock Quality
 * IEEE1588-2019 section 7.6.2.5, Table 4
 */
struct ptp_clock_quality {
    /// The clock class. Default is 248, for slave-only the value is 255.
    uint8_t clock_class {};
    ptp_clock_accuracy clock_accuracy {};
    uint16_t offset_scaled_log_variance {};
};

}
