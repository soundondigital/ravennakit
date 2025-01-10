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
#include "ravennakit/ptp/types/ptp_time_interval.hpp"

namespace rav {

template<class T>
struct ptp_measurement {
    T sync_event_ingress_timestamp;
    T offset_from_master;
    T mean_delay;
    T corrected_master_event_timestamp;
};

}
