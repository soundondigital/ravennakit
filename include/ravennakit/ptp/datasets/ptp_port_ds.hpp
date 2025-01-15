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
#include "ravennakit/ptp/ptp_profiles.hpp"
#include "ravennakit/ptp/types/ptp_port_identity.hpp"
#include "ravennakit/ptp/types/ptp_timestamp.hpp"

namespace rav {

/**
 * Port data set. IEEE 1588-2019: 8.2.15.
 */
struct ptp_port_ds {
    ptp_port_identity port_identity;
    ptp_state port_state {ptp_state::undefined};
    /// Valid range: [0,5]
    int8_t log_min_delay_req_interval {0};  // Required for e2e only
    ptp_time_interval mean_link_delay;      // Required for p2p only

    /// Specifies the mean time interval between successive Announce messages. Should be uniform throughout a domain.
    /// IEEE 1588-2019: 7.7.2.2
    int8_t log_announce_interval {1};

    /// Number of announceIntervals. Should be uniform throughout a domain. Recommended is at least 3.
    /// IEEE 1588-2019: 7.7.3.1
    uint8_t announce_receipt_timeout {3};

    /// Sync interval.
    /// IEEE 1588-2019: 7.7.2.3
    int8_t log_sync_interval {1};

    ptp_delay_mechanism delay_mechanism {};  // Required for p2p only
    int8_t log_min_pdelay_req_interval {0};  // Required for p2p only
    uint8_t version_number {2};              // 4 bits on the wire (one nibble)
    uint8_t minor_version_number {1};        // 4 bits on the wire (one nibble)
    ptp_time_interval delay_asymmetry;

    /**
     * Checks the internal state of this object according to IEEE1588-2019. Asserts when something is wrong.
     * @param profile The profile to check against.
     */
    void assert_valid_state(const ptp_profile& profile) const {
        port_identity.assert_valid_state();
        RAV_ASSERT(port_state != ptp_state::undefined, "port_state is undefined");
        RAV_ASSERT(
            profile.port_ds.log_announce_interval_range.contains(log_announce_interval),
            "log_announce_interval is out of range"
        );
        RAV_ASSERT(
            profile.port_ds.log_sync_interval_range.contains(log_sync_interval), "log_sync_interval is out of range"
        );
        RAV_ASSERT(
            profile.port_ds.log_min_delay_req_interval_range.contains(log_min_delay_req_interval),
            "log_min_delay_req_interval is out of range"
        );
        RAV_ASSERT(
            profile.port_ds.announce_receipt_timeout_range.contains(announce_receipt_timeout),
            "announce_receipt_timeout is out of range"
        );
        if (profile.port_ds.log_pdelay_req_interval_default.has_value()) {
            RAV_ASSERT(
                profile.port_ds.log_pdelay_req_interval_range.value().contains(log_min_pdelay_req_interval),
                "log_min_pdelay_req_interval is out of range"
            );
        }
    }
};

}  // namespace rav
