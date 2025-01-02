/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include "ravennakit/core/chrono/high_resolution_clock.hpp"
#include "types/ptp_timestamp.hpp"

#include <cstdint>

namespace rav {

/**
 * A class that maintains a local clock.
 * This implementation is a simple wrapper around the high_resolution_clock.
 */
class ptp_local_clock {
  public:
    [[nodiscard]] static ptp_timestamp now() {
        return ptp_timestamp(high_resolution_clock::now());
    }
};

}  // namespace rav
