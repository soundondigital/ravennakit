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

#include "ptp_local_clock.hpp"
#include "ravennakit/core/tracy.hpp"
#include "ravennakit/core/math/running_average.hpp"
#include "types/ptp_timestamp.hpp"

#include <cstdint>

namespace rav {

/**
 * A class that maintains a local PTP clock as close as possible to some grand master clock.
 * This particular implementation maintains a 'virtual' clock based on the local clock and a correction value.
 */
class ptp_local_ptp_clock {
  public:
    ptp_local_ptp_clock() : last_sync_(ptp_local_clock::now()) {}

    [[nodiscard]] ptp_timestamp now() const {
        auto now = ptp_local_clock::now();
        now.add(shift_);
        return now;
    }

    [[nodiscard]] bool is_calibrated() const {
        return false;  // TODO: Implement calibration
    }

    void adjust(const ptp_time_interval offset_from_master) {
        last_sync_ = ptp_local_clock::now();
        shift_ += offset_from_master * -1;
        offset_average_.add(offset_from_master.nanos());
        TRACY_PLOT("Offset from master (avg)", offset_average_.average());
    }

    void step_clock(const ptp_time_interval offset_from_master) {
        last_sync_ = ptp_local_clock::now();
        shift_ += offset_from_master * -1;
        offset_average_.reset();
        TRACY_MESSAGE("Clock stepped");
    }

  private:
    ptp_timestamp last_sync_ {};  // Timestamp from ptp_local_clock when the clock was last synchronized
    ptp_time_interval shift_ {};
    running_average offset_average_;
};

}  // namespace rav
