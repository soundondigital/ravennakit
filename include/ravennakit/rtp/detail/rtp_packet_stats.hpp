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

#include "ravennakit/core/tracy.hpp"
#include "ravennakit/core/containers/ring_buffer.hpp"
#include "ravennakit/core/util/wrapping_uint.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"

namespace rav {

/**
 * A class that collects statistics about RTP packets.
 */
class rtp_packet_stats {
  public:
    struct counters {
        /// The number of packets which arrived out of order, not including duplicate packets.
        uint32_t out_of_order {};
        /// The number of packets which were duplicates.
        uint32_t duplicates {};
        /// The number of packets which were dropped.
        uint32_t dropped {};
        /// The number of packets which were too late for consumer.
        uint32_t too_late {};

        [[nodiscard]] auto tie() const {
            return std::tie(out_of_order, too_late, duplicates, dropped);
        }

        friend bool operator==(const counters& lhs, const counters& rhs) {
            return lhs.tie() == rhs.tie();
        }

        friend bool operator!=(const counters& lhs, const counters& rhs) {
            return lhs.tie() != rhs.tie();
        }

        counters operator+(const counters other) const {
            counters result = *this;
            result.out_of_order += other.out_of_order;
            result.too_late += other.too_late;
            result.duplicates += other.duplicates;
            result.dropped += other.dropped;
            return result;
        }

        std::string to_string() {
            return fmt::format(
                "out_of_order: {}, duplicates: {}, dropped: {}, too_late: {}", out_of_order, duplicates, dropped,
                too_late
            );
        }
    };

    explicit rtp_packet_stats() = default;

    /**
     * Updates the statistics with the given packet.
     * @param sequence_number
     * @return Returns the total counts.
     */
    std::optional<counters> update(const uint16_t sequence_number) {
        const auto packet_sequence_number = wrapping_uint16(sequence_number);

        if (!most_recent_sequence_number_) {
            most_recent_sequence_number_ = packet_sequence_number;
            return std::nullopt;
        }

        if (packet_sequence_number <= most_recent_sequence_number_) {
            if (remove_dropped(sequence_number)) {
                --totals_.dropped;
                ++totals_.out_of_order;
            } else {
                ++totals_.duplicates;
            }
            dirty_ = false;
            return totals_;
        }

        if (const auto diff = most_recent_sequence_number_->update(sequence_number)) {
            clear_outdated_dropped_packets();

            for (uint16_t i = 1; i < *diff; i++) {
                ++totals_.dropped;
                dropped_packets_.push_back(sequence_number - i);
                dirty_ = true;
            }

            if (dirty_) {
                dirty_ = false;
                return totals_;
            }
        }

        // mark_packet_too_late might have set the dirty flag
        if (std::exchange(dirty_, false)) {
            return totals_;
        }

        return std::nullopt;
    }

    /**
     * Marks a packet as too late which means it didn't arrive in time for the consumer.
     */
    void mark_packet_too_late(const uint16_t sequence_number) {
        if (!most_recent_sequence_number_) {
            return;  // Can't mark a packet too late which never arrived
        }
        const auto packet_sequence_number = wrapping_uint16(sequence_number);
        if (packet_sequence_number > *most_recent_sequence_number_) {
            return;  // Packet is newer, or older than half the range of uint16
        }
        totals_.too_late++;
        dirty_ = true;
    }

    /**
     * @return The total counts. These are the collected numbers plus the ones in the window.
     */
    [[nodiscard]] counters get_total_counts() const {
        return totals_;
    }

    /**
     * Resets to the initial state.
     */
    void reset() {
        most_recent_sequence_number_ = {};
        totals_ = {};
    }

  private:
    std::optional<wrapping_uint16> most_recent_sequence_number_ {};
    counters totals_ {};
    bool dirty_ {};
    std::vector<uint16_t> dropped_packets_ {};

    bool remove_dropped(const uint16_t sequence_number) {
        // TODO: Update with swap + delete trick
        for (auto it = dropped_packets_.begin(); it != dropped_packets_.end(); ++it) {
            if (*it == sequence_number) {
                dropped_packets_.erase(it);
                return true;
            }
        }

        return false;
    }

    void clear_outdated_dropped_packets() {
        const auto most_recent = *most_recent_sequence_number_;
        for (auto it = dropped_packets_.begin(); it != dropped_packets_.end();) {
            // If a packet is newer than the most recent packet, it's older than half the range of uint16.
            if (wrapping_uint16(*it) > most_recent) {
                it = dropped_packets_.erase(it);
            } else {
                ++it;
            }
        }
    }
};

}  // namespace rav
