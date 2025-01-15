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

#include "ravennakit/ptp/ptp_definitions.hpp"

#include <cstdint>

namespace rav {

/**
 * Represents the time properties data set as described in IEEE1588-2019: 8.2.4.
 */
struct ptp_time_properties_ds {
    int16_t current_utc_offset {};
    bool current_utc_offset_valid {};
    bool leap59 {};
    bool leap61 {};
    bool time_traceable {};
    bool frequency_traceable {};
    bool ptp_timescale {};
    ptp_time_source time_source {};
};

}  // namespace rav
