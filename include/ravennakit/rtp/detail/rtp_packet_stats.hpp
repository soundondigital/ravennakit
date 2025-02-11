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
        /// The number of packets which arrived out of order.
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
     * @param window_size The window size in number of packets. Max value is 65535 (0xffff).
     */
    explicit rtp_packet_stats(const size_t window_size) {
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
            most_recent_sequence_number_ = packet_sequence_number - 1;
        }

        if (packet_sequence_number <= *most_recent_sequence_number_ - static_cast<uint16_t>(window_.size())) {
            collected_total_counts_.outside_window++;  // Too old for the window
            if (std::exchange(dirty_, false)) {
                return collected_total_counts_ + get_window_counts();
            }
            return std::nullopt;
        }

        if (window_.capacity() == 0) {
            RAV_ASSERT_FALSE("Window is has zero capacity");
            return {};
        }

        if (const auto diff = most_recent_sequence_number_->update(sequence_number)) {
            for (uint16_t i = 0; i < *diff; i++) {
                if (window_.full()) {
                    collect_oldest_packet();
                }
                std::ignore = window_.push_back({});
            }
            window_.back().times_received++;
        } else {
            auto& p = window_[window_.size() - 1 - (*most_recent_sequence_number_ - sequence_number).value()];
            p.times_out_of_order++;  // Packet out of order
            p.times_received++;
        }

        if (std::exchange(dirty_, false)) {
            return collected_total_counts_ + get_window_counts();
        }

        return std::nullopt;
    }

    /**
     * Collects the statistics for the current window.
     * @return The collected statistics.
     */
    [[nodiscard]] counters get_window_counts() const {
        if (window_.empty()) {
            return {};
        }

        if (!most_recent_sequence_number_) {
            return {};  // No packets received yet
        }

        counters result {};
        for (auto& p : window_) {
            if (p.times_received == 0) {
                result.dropped++;
            } else if (p.times_received > 1) {
                result.duplicates += p.times_received - 1;
            }
            result.out_of_order += p.times_out_of_order;
            result.too_late += p.times_too_late;
        }
        return result;
    }

    /**
     * @return The total counts. These are the collected numbers plus the ones in the window.
     */
    [[nodiscard]] counters get_total_counts() const {
        return collected_total_counts_ + get_window_counts();
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
        if (packet_sequence_number <= *most_recent_sequence_number_ - static_cast<uint16_t>(window_.size())) {
            return;  // Too old for the window
        }
        auto& p = window_[window_.size() - 1 - (*most_recent_sequence_number_ - sequence_number).value()];
        p.times_too_late++;
        dirty_ = true;
    }

    /**
     * @return The number of packets in the window.
     */
    [[nodiscard]] size_t count() const {
        return window_.size();
    }

    /**
     * Resets to the initial state.
     * @param window_size The window size in number of packets. Max value is 65535 (0xffff).
     */
    void reset(const std::optional<size_t> window_size = {}) {
        if (window_size.has_value()) {
            RAV_ASSERT(
                window_size <= std::numeric_limits<uint16_t>::max(),
                "Since a sequence number will wrap around at 0xffff, the window size can't be larger than that"
            );
            window_.reset(*window_size);
        }
        most_recent_sequence_number_ = {};
        collected_total_counts_ = {};
    }

  private:
    struct packet {
        uint16_t times_received {};
        uint16_t times_out_of_order {};
        uint16_t times_too_late {};
    };

    std::optional<wrapping_uint16> most_recent_sequence_number_ {};
    ring_buffer<packet> window_ {};
    counters collected_total_counts_ {};
    bool dirty_ {};

    void collect_oldest_packet() {
        const auto pkt = window_.pop_front();
        RAV_ASSERT(pkt.has_value(), "No packet to collect");

        if (pkt->times_received == 0) {
            collected_total_counts_.dropped++;
            dirty_ = true;
        }
        if (pkt->times_received > 1) {
            collected_total_counts_.duplicates += pkt->times_received - 1u;
            dirty_ = true;
        }
        if (pkt->times_out_of_order > 0) {
            collected_total_counts_.out_of_order += pkt->times_out_of_order;
            dirty_ = true;
        }
        if (pkt->times_too_late > 0) {
            collected_total_counts_.too_late += pkt->times_too_late;
            dirty_ = true;
        }
    }
};

}  // namespace rav
