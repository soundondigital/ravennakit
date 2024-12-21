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

#include "ptp_comparison_data_set.hpp"
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

    ptp_foreign_master_list() = default;

    /**
     * Adds or updates an entry in the foreign master list.
     * @param announce_message The announce message to add or update.
     */
    void add_or_update_entry(const ptp_announce_message& announce_message) {
        const auto foreign_port_identity = announce_message.header.source_port_identity;

        if (auto* entry = find_entry(foreign_port_identity)) {
            // IEEE 1588-2019: 9.3.2.5.b If message is not the most recent one, it is not qualified.
            if (entry->most_recent_announce_message) {
                if (announce_message.header.sequence_id <= entry->most_recent_announce_message->header.sequence_id) {
                    RAV_WARNING("Discarding announce message because it is not the most recent one");
                    return;
                }
            }

            entry->foreign_master_announce_messages++;

            // TODO: Find a way to implement the time window.

            // IEEE 1588-2019: 9.3.2.5.c If fewer than k_foreign_master_threshold messages have been received from the
            // foreign master within the most recent k_foreign_master_time_window, message is not qualified.
            if (entry->foreign_master_announce_messages < k_foreign_master_threshold) {
                return;
            }

            // IEEE 1588-2019: 9.3.2.5.e Otherwise, the message is qualified.
            entry->most_recent_announce_message = announce_message;
        } else {
            // IEEE 1588-2019: 9.5.3.b states that a new records start with 0 announce messages.
            entries_.push_back({foreign_port_identity, 0, {}});
        }
    }

    /**
     * Removes all entries except the one with the given foreign master port identity.
     * @param foreign_master_port_identity The foreign master port identity to keep.
     */
    void remove_all_except_for(const ptp_port_identity& foreign_master_port_identity) {
        entries_.erase(
            std::remove_if(
                entries_.begin(), entries_.end(),
                [&foreign_master_port_identity](const entry& entry) {
                    return entry.foreign_master_port_identity != foreign_master_port_identity;
                }
            ),
            entries_.end()
        );
    }

    /**
     * Removes all entries except the one with the given foreign master port identity.
     * @param except_for_announce_message The entry for which to keep the foreign master port identity.
     */
    void remove_all_except_for(const std::optional<ptp_announce_message>& except_for_announce_message) {
        if (except_for_announce_message) {
            entries_.erase(
                std::remove_if(
                    entries_.begin(), entries_.end(),
                    [&except_for_announce_message](const entry& entry) {
                        return entry.foreign_master_port_identity
                            != except_for_announce_message->header.source_port_identity;
                    }
                ),
                entries_.end()
            );
        } else {
            clear();
        }
    }

    /**
     * Clears the foreign master list.
     */
    void clear() {
        entries_.clear();
    }

    [[nodiscard]] std::vector<entry>::const_iterator begin() const {
        return entries_.begin();
    }

    [[nodiscard]] std::vector<entry>::const_iterator end() const {
        return entries_.end();
    }

  private:
    std::vector<entry> entries_;

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
