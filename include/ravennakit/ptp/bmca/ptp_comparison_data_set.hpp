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
#include "ravennakit/ptp/datasets/ptp_default_ds.hpp"
#include "ravennakit/ptp/datasets/ptp_port_ds.hpp"
#include "ravennakit/ptp/messages/ptp_announce_message.hpp"
#include "ravennakit/ptp/types/ptp_clock_quality.hpp"
#include "ravennakit/ptp/types/ptp_port_identity.hpp"

#include <cstdint>

namespace rav {

/**
 * Contains the data needed to compare two PTP data sets.
 */
struct ptp_comparison_data_set {
    uint8_t grandmaster_priority1 {};
    ptp_clock_identity grandmaster_identity;
    ptp_clock_quality grandmaster_clock_quality;
    uint8_t grandmaster_priority2 {};
    uint16_t steps_removed {};
    ptp_clock_identity identity_of_senders;
    ptp_port_identity identity_of_receiver;

    /**
     * The result of comparing two PTP data sets.
     * Note: the order of the enum values is important for the comparison logic.
     */
    enum class result {
        worse,
        /// The set is worse than the one being compared to.
        /// The set is of equal quality as the one being compared to, but is worse by the topology.
        worse_by_topology,
        /// Both sets are equal. Indicates that one of the PTP messages was transmitted and received on the same PTP
        /// Port.
        error1,
        /// Both sets are equal. Indicates that the PTP messages are duplicates or that they are an earlier and later
        /// PTP message from the
        /// same Grandmaster PTP Instance
        error2,
        /// The set is of equal quality as the one being compared to, but is preferred by the topology.
        better_by_topology,
        /// The set is better than the one being compared to.
        better,
    };

    ptp_comparison_data_set() = default;

    /**
     * Constructs a set from an announce message and a port data set.
     * @param announce_message The announce message.
     * @param receiver_identity The identity of the receiver.
     */
    ptp_comparison_data_set(const ptp_announce_message& announce_message, const ptp_port_identity& receiver_identity);

    /**
     * Constructs a set from an announce message and a port data set.
     * @param announce_message The announce message.
     * @param port_ds The port data set.
     */
    ptp_comparison_data_set(const ptp_announce_message& announce_message, const ptp_port_ds& port_ds);

    /**
     * Constructs a set from a default data set.
     * @param default_ds The default data set.
     */
    explicit ptp_comparison_data_set(const ptp_default_ds& default_ds);

    /**
     * Compares this data set to another. The comparison is done according to the rules in IEEE 1588-2019 9.3.4.
     * @param other The other data set to compare to.
     * @return The result of the comparison. See the result enum for more information.
     */
    [[nodiscard]] result compare(const ptp_comparison_data_set& other) const;

    /**
     * Convenience method for comparing two announce messages.
     * @param a The first announce message.
     * @param b The second announce message.
     * @param receiver_identity The identity of the receiver.
     * @return The result of the comparison. See the result enum for more information.
     */
    static result
    compare(const ptp_announce_message& a, const ptp_announce_message& b, const ptp_port_identity& receiver_identity);
};

}  // namespace rav
