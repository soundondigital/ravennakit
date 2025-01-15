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
        /// The age of the most recent announce message.
        size_t age {};
    };

    ptp_foreign_master_list() = default;

    /**
     * Adds or updates an entry in the foreign master list.
     * @param announce_message The announce message to add or update.
     */
    void add_or_update_entry(const ptp_announce_message& announce_message) {
        const auto foreign_port_identity = announce_message.header.source_port_identity;

        if (auto* e = find_entry(foreign_port_identity)) {
            // IEEE 1588-2019: 9.3.2.5.b If message is not the most recent one, it is not qualified.
            if (e->most_recent_announce_message) {
                if (announce_message.header.sequence_id < e->most_recent_announce_message->header.sequence_id) {
                    RAV_WARNING("Discarding announce message because it is not the most recent one");
                    return;
                }
            }

            // IEEE 1588-2019: 9.3.2.5.e Otherwise, the message is qualified.

            e->foreign_master_announce_messages++;
            e->most_recent_announce_message = announce_message;
            e->age = 0;
        } else {
            // IEEE 1588-2019: 9.5.3.b states that a new records start with 0 announce messages.
            entries_.push_back({foreign_port_identity, 0, {}, 0});
        }
    }

    /**
     * Removes all entries except the one with the given foreign master port identity.
     * @param erbest The entry for which to keep the foreign master port identity.
     */
    void purge_entries(const std::optional<ptp_announce_message>& erbest) {
        entries_.erase(
            std::remove_if(
                entries_.begin(), entries_.end(),
                [&erbest](const entry& e) {
                    if (erbest && erbest->header.source_port_identity == e.foreign_master_port_identity) {
                        return false;  // Keep the entry for the new Erbest.
                    }
                    if (e.age > k_foreign_master_time_window) {
                        return true;  // Entry is too old, so we remove it.
                    }
                    if (e.foreign_master_announce_messages < k_foreign_master_threshold) {
                        return false;  // Message is not qualified, so we don't remove it.
                    }
                    return true;
                }
            ),
            entries_.end()
        );
    }

    /**
     * Clears the foreign master list.
     */
    void clear() {
        entries_.clear();
    }

    /**
     * @return The number of entries in the foreign master list.
     */
    [[nodiscard]] size_t size() const {
        return entries_.size();
    }

    [[nodiscard]] std::vector<entry>::const_iterator begin() const {
        return entries_.begin();
    }

    [[nodiscard]] std::vector<entry>::const_iterator end() const {
        return entries_.end();
    }

    /**
     * Increases the age of all entries in the foreign master list.
     */
    void increase_age() {
        for (auto& e : entries_) {
            e.age++;
            const auto max_num_messages_in_window = k_foreign_master_time_window - e.age;
            if (e.foreign_master_announce_messages > max_num_messages_in_window) {
                e.foreign_master_announce_messages = max_num_messages_in_window;
            }
        }
    }

  private:
    std::vector<entry> entries_;

    [[nodiscard]] const entry* find_entry(const ptp_port_identity& foreign_master_port_identity) const {
        for (auto& e : entries_) {
            if (e.foreign_master_port_identity == foreign_master_port_identity) {
                return &e;
            }
        }

        return nullptr;
    }

    [[nodiscard]] entry* find_entry(const ptp_port_identity& foreign_master_port_identity) {
        for (auto& e : entries_) {
            if (e.foreign_master_port_identity == foreign_master_port_identity) {
                return &e;
            }
        }

        return nullptr;
    }
};

}  // namespace rav
