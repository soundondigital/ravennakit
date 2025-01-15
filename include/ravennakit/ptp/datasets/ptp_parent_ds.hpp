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

#include "ptp_default_ds.hpp"
#include "ravennakit/ptp/messages/ptp_announce_message.hpp"
#include "ravennakit/ptp/types/ptp_clock_quality.hpp"
#include "ravennakit/ptp/types/ptp_port_identity.hpp"

namespace rav {

/**
 * Represents the parent data set as described in IEEE1588-2019: 8.2.3.
 */
struct ptp_parent_ds {
    ptp_port_identity parent_port_identity;
    bool parent_stats {};
    ptp_clock_identity grandmaster_identity;
    ptp_clock_quality grandmaster_clock_quality;
    uint16_t grandmaster_priority1 {};
    uint8_t grandmaster_priority2 {};

    ptp_parent_ds() = default;

    explicit ptp_parent_ds(const ptp_default_ds& default_ds) {
        parent_port_identity.clock_identity = default_ds.clock_identity;  // IEEE1588-2019: 8.2.3.2
        parent_stats = false;                                             // IEEE1588-2019: 8.2.3.3
        grandmaster_identity = default_ds.clock_identity;                 // IEEE1588-2019: 8.2.3.6
        grandmaster_clock_quality = default_ds.clock_quality;             // IEEE1588-2019: 8.2.3.7
        grandmaster_priority1 = default_ds.priority1;                     // IEEE1588-2019: 8.2.3.8
        grandmaster_priority2 = default_ds.priority2;                     // IEEE1588-2019: 8.2.3.9
    }

    /**
     * @return A string representation of the parent data set.
     */
    [[nodiscard]] std::string to_string() const {
        return fmt::format(
            "Parent port identity: {}, grandmaster identity: {}, grandmaster priority1: {}, grandmaster priority2: {}",
            parent_port_identity.clock_identity.to_string(), grandmaster_identity.to_string(), grandmaster_priority1,
            grandmaster_priority2
        );
    }
};

}  // namespace rav
