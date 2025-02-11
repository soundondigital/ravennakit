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

#include "ravennakit/core/subscription.hpp"
#include "ravennakit/core/tracy.hpp"
#include "ravennakit/core/containers/ring_buffer.hpp"
#include "ravennakit/core/containers/detail/fifo.hpp"
#include "ravennakit/core/util/wrapping_uint.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"

#include <utility>

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
        /// The number of packets which were outside the window.
        uint32_t outside_window {};

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
            result.outside_window += other.outside_window;
            return result;
        }

        std::string to_string() {
            return fmt::format(
                "out_of_order: {}, duplicates: {}, dropped: {}, too_late: {}, outside_window: {}", out_of_order,
                duplicates, dropped, too_late, outside_window
            );
        }
    };

    explicit rtp_packet_stats() = default;

    /**
     * @param window_size The window size in number of packets. Max value is 32766 (0x7fff).
     */
    explicit rtp_packet_stats(const uint16_t window_size) {
        reset(window_size);
    }

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

        RAV_ASSERT(window_size_ > 0, "Window size must be greater than zero");

        if (packet_sequence_number <= most_recent_sequence_number_) {
            if (!remove_skipped(sequence_number)) {
                totals_.duplicates++;
            } else {
                totals_.out_of_order++;
            }
            dirty_ = false;
            return totals_;
        }

        if (const auto diff = most_recent_sequence_number_->update(sequence_number)) {
            for (uint16_t i = 1; i < *diff; i++) {
                skipped_packets_.push_back(sequence_number - i);
            }
        } else {
            auto window_start = *most_recent_sequence_number_ - window_size_;
            if (packet_sequence_number <= window_start) {
                totals_.outside_window++;  // Too old for the window
                dirty_ = false;
                return totals_;
            }

            totals_.out_of_order++;  // Packet out of order
        }

        if (const auto skipped = count_remove_skipped(); skipped > 0) {
            totals_.dropped += skipped;
            dirty_ = true;
        }

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
            return;  // Can't mark a packet too late which is newer than the most recent packet
        }
        if (packet_sequence_number < *most_recent_sequence_number_ - window_size_) {
            return;  // Too old for the window
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
     * @param window_size The window size in number of packets. Max value is 32766 (0x7fff).
     */
    void reset(const std::optional<uint16_t> window_size = {}) {
        if (window_size.has_value()) {
            window_size_ = *window_size;
        }
        most_recent_sequence_number_ = {};
        totals_ = {};
    }

  private:
    std::optional<wrapping_uint16> most_recent_sequence_number_ {};
    counters totals_ {};
    bool dirty_ {};
    uint16_t window_size_ {};
    std::vector<uint16_t> skipped_packets_ {};

    bool remove_skipped(const uint16_t sequence_number) {
        // TODO: Update with swap + delete trick
        for (auto it = skipped_packets_.begin(); it != skipped_packets_.end(); ++it) {
            if (*it == sequence_number) {
                skipped_packets_.erase(it);
                return true;
            }
        }

        return false;
    }

    uint16_t count_remove_skipped() {
        const auto outdated = *most_recent_sequence_number_ - window_size_;
        uint16_t count = 0;
        for (auto it = skipped_packets_.begin(); it != skipped_packets_.end();) {
            if (wrapping_uint16(*it) <= outdated) {
                it = skipped_packets_.erase(it);
                count++;
            } else {
                ++it;
            }
        }
        return count;
    }
};

}  // namespace rav
