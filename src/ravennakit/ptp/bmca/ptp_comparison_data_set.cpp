/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/ptp/bmca/ptp_comparison_data_set.hpp"

rav::ptp_comparison_data_set::ptp_comparison_data_set(
    const ptp_announce_message& announce_message, const ptp_port_identity& receiver_identity
) {
    grandmaster_priority1 = announce_message.grandmaster_priority1;
    grandmaster_identity = announce_message.grandmaster_identity;
    grandmaster_clock_quality = announce_message.grandmaster_clock_quality;
    grandmaster_priority2 = announce_message.grandmaster_priority2;
    steps_removed = announce_message.steps_removed;
    identity_of_senders = announce_message.header.source_port_identity.clock_identity;
    identity_of_receiver = receiver_identity;
}

rav::ptp_comparison_data_set::ptp_comparison_data_set(
    const ptp_announce_message& announce_message, const ptp_port_ds& port_ds
) {
    grandmaster_priority1 = announce_message.grandmaster_priority1;
    grandmaster_identity = announce_message.grandmaster_identity;
    grandmaster_clock_quality = announce_message.grandmaster_clock_quality;
    grandmaster_priority2 = announce_message.grandmaster_priority2;
    steps_removed = announce_message.steps_removed;
    identity_of_senders = announce_message.header.source_port_identity.clock_identity;
    identity_of_receiver = port_ds.port_identity;
}

rav::ptp_comparison_data_set::ptp_comparison_data_set(const ptp_default_ds& default_ds) {
    grandmaster_priority1 = default_ds.priority1;
    grandmaster_identity = default_ds.clock_identity;
    grandmaster_clock_quality = default_ds.clock_quality;
    grandmaster_priority2 = default_ds.priority2;
    steps_removed = 0;
    identity_of_senders = default_ds.clock_identity;
    identity_of_receiver = {default_ds.clock_identity, 0};
}

rav::ptp_comparison_data_set::result
rav::ptp_comparison_data_set::compare(const ptp_comparison_data_set& other) const {
    if (grandmaster_identity == other.grandmaster_identity) {
        // Compare Steps Removed of A and B:

        if (steps_removed > other.steps_removed + 1) {
            return result::worse;
        }

        if (steps_removed + 1 < other.steps_removed) {
            return result::better;
        }

        // Compare Steps Removed of A and B:

        if (steps_removed > other.steps_removed) {
            if (identity_of_receiver.clock_identity < identity_of_senders) {
                return result::worse;
            }
            if (identity_of_receiver.clock_identity > identity_of_senders) {
                return result::worse_by_topology;
            }
            return result::error1;
        }

        if (steps_removed < other.steps_removed) {
            if (other.identity_of_receiver.clock_identity < other.identity_of_senders) {
                return result::better;
            }
            if (other.identity_of_receiver.clock_identity > other.identity_of_senders) {
                return result::better_by_topology;
            }
            return result::error1;
        }

        // Compare Identities of Senders of A and B:

        if (identity_of_senders > other.identity_of_senders) {
            return result::worse_by_topology;
        }

        if (identity_of_senders < other.identity_of_senders) {
            return result::better_by_topology;
        }

        // Compare Port Numbers of Receivers of A and B:

        if (identity_of_receiver.port_number > other.identity_of_receiver.port_number) {
            return result::worse_by_topology;
        }

        if (identity_of_receiver.port_number < other.identity_of_receiver.port_number) {
            return result::better_by_topology;
        }

        return result::error2;
    }

    // GM Priority1
    if (grandmaster_priority1 < other.grandmaster_priority1) {
        return result::better;
    }
    if (grandmaster_priority1 > other.grandmaster_priority1) {
        return result::worse;
    }

    // GM Clock class
    if (grandmaster_clock_quality.clock_class < other.grandmaster_clock_quality.clock_class) {
        return result::better;
    }
    if (grandmaster_clock_quality.clock_class > other.grandmaster_clock_quality.clock_class) {
        return result::worse;
    }

    // GM Accuracy
    if (grandmaster_clock_quality.clock_accuracy < other.grandmaster_clock_quality.clock_accuracy) {
        return result::better;
    }
    if (grandmaster_clock_quality.clock_accuracy > other.grandmaster_clock_quality.clock_accuracy) {
        return result::worse;
    }

    // GM Offset scaled log variance
    if (grandmaster_clock_quality.offset_scaled_log_variance
        < other.grandmaster_clock_quality.offset_scaled_log_variance) {
        return result::better;
    }
    if (grandmaster_clock_quality.offset_scaled_log_variance
        > other.grandmaster_clock_quality.offset_scaled_log_variance) {
        return result::worse;
    }

    // GM Priority 2
    if (grandmaster_priority2 < other.grandmaster_priority2) {
        return result::better;
    }
    if (grandmaster_priority2 > other.grandmaster_priority2) {
        return result::worse;
    }

    // IEEE1588-2019: 7.5.2.4 Ordering of clockIdentity and portIdentity values
    if (grandmaster_identity.data > other.grandmaster_identity.data) {
        return result::better;
    }
    if (grandmaster_identity.data < other.grandmaster_identity.data) {
        return result::worse;
    }

    RAV_ASSERT(
        identity_of_senders != other.identity_of_senders, "Clock identity senders must not be equal at this point"
    );

    return result::error1;
}

rav::ptp_comparison_data_set::result rav::ptp_comparison_data_set::compare(
    const ptp_announce_message& a, const ptp_announce_message& b, const ptp_port_identity& receiver_identity
) {
    const ptp_comparison_data_set set_a(a, receiver_identity);
    const ptp_comparison_data_set set_b(b, receiver_identity);
    return set_a.compare(set_b);
}
