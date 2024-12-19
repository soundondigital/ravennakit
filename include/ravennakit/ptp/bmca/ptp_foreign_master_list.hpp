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

#include "ravennakit/core/util/sequence_number.hpp"
#include "ravennakit/ptp/messages/ptp_announce_message.hpp"
#include "ravennakit/ptp/types/ptp_port_identity.hpp"

#include <vector>

namespace rav {

class ptp_foreign_master_list {
  public:
    static constexpr size_t k_foreign_master_time_window = 4;  // 4 announce intervals
    static constexpr size_t k_foreign_master_threshold = 2;    // 2 announce messages within the time window

    struct entry {
        /// The identity of the foreign master.
        ptp_port_identity foreign_master_port_identity;
        /// number of messages received within k_foreign_master_time_window.
        size_t foreign_master_announce_messages {};
        /// The most recent announce message received from the foreign master.
        std::optional<ptp_announce_message> most_recent_announce_message;
    };

    explicit ptp_foreign_master_list(const ptp_port_identity& port_identity) : port_identity_(port_identity) {}

    /**
     * Implementation of IEEE1588-2019: 9.3.2.5 Qualification of Announce messages
     * TODO: This should probably become a method of entry.
     * @param announce_message The announce message to test.
     * @return True if the announce message is qualified, false otherwise.
     */
    [[nodiscard]] bool is_announce_message_qualified(const ptp_announce_message& announce_message) const {
        const auto foreign_port_identity = announce_message.header.source_port_identity;

        // IEEE 1588-2019: 9.3.2.5

        // a) If a message comes from the same PTP instance, the message is not qualified. Since every port in the PTP
        // instance has the same clock identity, we can just compare that.
        if (foreign_port_identity.clock_identity == port_identity_.clock_identity) {
            RAV_TRACE("Message is not qualified because it comes from the same PTP instance");
            return false;
        }

        // b) If message is not the most recent one from the foreign master, it is not qualified.
        if (auto* entry = find_entry(foreign_port_identity)) {
            if (const auto previous = entry->most_recent_announce_message) {
                if (sequence_number(announce_message.header.sequence_id)
                    <= sequence_number(previous->header.sequence_id)) {
                    RAV_TRACE("Message is not qualified because it is not the most recent one from the foreign master");
                    return false;
                }
            }
        }

        // c) If fewer than k_foreign_master_threshold messages have been received from the foreign master within the
        // most recent k_foreign_master_time_window, message is not qualified.
        if (auto* entry = find_entry(foreign_port_identity)) {
            if (entry->foreign_master_announce_messages < k_foreign_master_threshold) {
                RAV_TRACE(
                    "Message is not qualified because fewer than {} messages have been received from the foreign master",
                    k_foreign_master_threshold
                );
                return false;
            }
        }

        // d) Discard messages with steps removed of 255 or greater
        if (announce_message.steps_removed >= 255) {
            RAV_TRACE("Message is not qualified because steps removed is 255 or greater");
            return false;
        }

        return true;
    }

    /**
     * Adds or updates an entry in the foreign master list.
     * @param announce_message The announce message to add or update.
     */
    void add_or_update_entry(const ptp_announce_message& announce_message) {
        const auto foreign_port_identity = announce_message.header.source_port_identity;

        if (auto* entry = find_entry(foreign_port_identity)) {
            entry->foreign_master_announce_messages++;
            entry->most_recent_announce_message = announce_message;
        } else {
            // IEEE 1588-2019: 9.5.3 b) states that a new records start with 0 announce messages.
            entries_.push_back({foreign_port_identity, 0, announce_message});
        }
    }

  private:
    std::vector<entry> entries_;
    ptp_port_identity port_identity_;

    [[nodiscard]] const entry* find_entry(const ptp_port_identity& foreign_master_port_identity) const {
        for (auto& entry : entries_) {
            if (entry.foreign_master_port_identity == foreign_master_port_identity) {
                return &entry;
            }
        }

        return nullptr;
    }

    [[nodiscard]] entry* find_entry(const ptp_port_identity& foreign_master_port_identity) {
        for (auto& entry : entries_) {
            if (entry.foreign_master_port_identity == foreign_master_port_identity) {
                return &entry;
            }
        }

        return nullptr;
    }
};

}  // namespace rav
