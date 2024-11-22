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

#include "rtp_receive_buffer.hpp"

namespace rav {

class rtp_depacketizer {
  public:
    enum class status {
        ok,
        packet_too_old,
        packet_too_new,
        packet_out_of_order,
        packet_gap,
    };

    explicit rtp_depacketizer(rtp_receive_buffer& buffer) : buffer_(buffer) {}

    status handle_rtp_packet(const rtp_packet_view& packet, const uint32_t delay) {
        if (packet.sequence_number() > sequence_number_ + 1) {
            // Missing one or more packets. Clear buffer until packet.timestamp(). Clearing will only happen once to
            // prevent clearing packets which might have already been written by a redundant stream.
            buffer_.clear_until(packet.timestamp());
        }

        if (packet.sequence_number() > sequence_number_) {
            sequence_number_ = packet.sequence_number();
        } else {
            // It's an old packet, but we want to place it in the buffer anyway because the spot might have been only
            // cleared.
        }

        // Discard packet if it's too old
        if (packet.timestamp() /* + packet_time */ < buffer_.next_ts() - delay) {
            return status::packet_too_old;
        }
        // If packet is not too old < current_timestamp_ - delay

        return status::ok;
    }

  private:
    rtp_receive_buffer& buffer_;
    uint32_t sequence_number_ = 0;
};

}  // namespace rav