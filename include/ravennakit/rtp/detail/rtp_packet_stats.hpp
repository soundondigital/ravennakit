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
#include "ravennakit/core/containers/ring_buffer.hpp"
#include "ravennakit/core/containers/detail/fifo.hpp"
#include "ravennakit/core/util/wrapping_uint.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"

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
        /// The number of packets which were too old for the window.
        uint32_t too_old {};

        [[nodiscard]] auto tie() const {
            return std::tie(out_of_order, too_late, duplicates, dropped);
        }

        friend bool operator==(const counters& lhs, const counters& rhs) {
            return lhs.tie() == rhs.tie();
        }

        friend bool operator!=(const counters& lhs, const counters& rhs) {
            return lhs.tie() != rhs.tie();
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
     * @return True if packet should be processed, or false if it should be dropped because it is too old.
     */
    void update(const uint16_t sequence_number) {
        const auto packet_sequence_number = wrapping_uint16(sequence_number);

        if (!most_recent_sequence_number_) {
            most_recent_sequence_number_ = packet_sequence_number - 1;
        }

        if (packet_sequence_number <= *most_recent_sequence_number_ - static_cast<uint16_t>(window2_.size())) {
            total_counts_.too_old++;
            return;  // Too old for the window
        }

        auto& packet = window2_[sequence_number % window2_.size()];

        if (const auto diff = most_recent_sequence_number_->update(sequence_number)) {
            if (count_ + *diff > window2_.size()) {
                for (size_t i = 0; i < count_ + *diff - window2_.size(); i++) {
                    collect_packet((*most_recent_sequence_number_ - static_cast<uint16_t>(i) - static_cast<uint16_t>(window2_.size())).value()
                    );
                    count_--;
                }
            }
            count_ += *diff;
        } else {
            packet.times_out_of_order++;  // Packet out of order
        }

        packet.times_received++;
    }

    /**
     * Collects the statistics for the current window.
     * @return The collected statistics.
     */
    [[nodiscard]] counters get_window_counts() const {
        if (count_ == 0) {
            return {};
        }

        if (!most_recent_sequence_number_) {
            return {};  // No packets received yet
        }

        counters result {};
        for (auto i = *most_recent_sequence_number_ - count_ + 1; i <= *most_recent_sequence_number_; i += 1) {
            const auto& packet = window2_[i.value() % window2_.size()];
            if (packet.times_received == 0) {
                result.dropped++;
            } else if (packet.times_received > 1) {
                result.duplicates += packet.times_received - 1;
            }
            result.out_of_order += packet.times_out_of_order;
            result.too_late += packet.times_too_late;
        }
        return result;
    }

    /**
     * @return The total counts. This will only contain the numbers of packets which are moved out of the window.
     */
    [[nodiscard]] counters get_total_counts() const {
        return total_counts_;
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
        if (packet_sequence_number <= *most_recent_sequence_number_ - count_) {
            return;  // Too old for the window
        }
        window2_[sequence_number % window2_.size()].times_too_late++;
    }

    /**
     * @return The number of packets in the window.
     */
    [[nodiscard]] size_t count() const {
        return count_;
    }

    /**
     * Resets to the initial state.
     * @param window_size The window size in number of packets. Max value is 65535 (0xffff).
     */
    void reset(const std::optional<uint16_t> window_size = {}) {
        if (window_size.has_value()) {
            RAV_ASSERT(
                window_size <= std::numeric_limits<uint16_t>::max(),
                "Since a sequence number will wrap around at 0xffff, the window size can't be larger than that"
            );
            window2_.resize(*window_size);
        }
        std::fill(window2_.begin(), window2_.end(), packet {});
        count_ = 0;
        most_recent_sequence_number_ = {};
        total_counts_ = {};
    }

  private:
    struct packet {
        uint16_t times_received {};
        uint16_t times_out_of_order {};
        uint16_t times_too_late {};
    };

    std::optional<wrapping_uint16> most_recent_sequence_number_ {};
    std::vector<packet> window2_ {};
    uint16_t count_ {};  // Number of values currently in the window
    counters total_counts_ {};

    bool collect_packet(const uint16_t sequence_number) {
        auto& pkt = window2_[sequence_number % window2_.size()];
        if (pkt.times_received == 0) {
            total_counts_.dropped++;
        }
        if (pkt.times_received > 1) {
            total_counts_.duplicates += pkt.times_received - 1;
        }
        if (pkt.times_out_of_order > 0) {
            total_counts_.out_of_order += pkt.times_out_of_order;
        }
        if (pkt.times_too_late > 0) {
            total_counts_.too_late += pkt.times_too_late;
        }
        pkt = {};
        return true;
    }
};

}  // namespace rav
