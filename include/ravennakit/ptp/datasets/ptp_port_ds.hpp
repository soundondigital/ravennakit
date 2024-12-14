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

#include "ravennakit/ptp/ptp_types.hpp"
#include "ravennakit/ptp/types/ptp_port_identity.hpp"

namespace rav {

/**
 * Port data set. IEEE 1588-2019: 8.2.15.
 */
struct ptp_port_ds {
    ptp_port_identity port_identity;
    ptp_state port_state {ptp_state::undefined};
    /// Valid range: [0,5]
    int8_t log_min_delay_req_interval {0};  // Required for e2e only
    ptp_time_interval mean_link_delay {0};  // Required for p2p only
    /// Specifies the mean time interval between successive Announce messages. Range [0,4]. Should be uniform throughout
    /// a domain. IEEE 1588-2019: 7.7.2.2
    int8_t log_announce_interval {1};  // logarithm to the base 2 of the interval in seconds
    /// Number of announceIntervals in range [2,10]. Should be uniform throughout a domain. IEEE 1588-2019: 7.7.3.1
    uint8_t announce_receipt_timeout {3};  // Recommended is at least 3
    /// Valid range: [-1,1]. IEEE 1588-2019: I.3.2
    int8_t log_sync_interval {1};
    ptp_delay_mechanism delay_mechanism {};  // Required for p2p only
    int8_t log_min_pdelay_req_interval {0};  // Required for p2p only
    uint8_t version_number {2};              // 4 bits on the wire (one nibble)
    uint8_t minor_version_number {1};        // 4 bits on the wire (one nibble)
    ptp_time_interval delay_asymmetry {0};
};

}  // namespace rav
