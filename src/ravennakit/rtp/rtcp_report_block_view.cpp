/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/rtcp_report_block_view.hpp"
#include "ravennakit/core/byte_order.hpp"

#include <array>

rav::rtcp_report_block_view::rtcp_report_block_view(const uint8_t* data, const size_t size_bytes) :
    data_(data), size_bytes_(size_bytes) {}

bool rav::rtcp_report_block_view::validate() const {
    if (data_ == nullptr) {
        return false;
    }

    if (size_bytes_ < k_report_block_length_length) {
        return false;
    }

    if (size_bytes_ > k_report_block_length_length) {
        return false;
    }

    return true;
}

uint32_t rav::rtcp_report_block_view::ssrc() const {
    return byte_order::read_be<uint32_t>(data_);
}

uint8_t rav::rtcp_report_block_view::fraction_lost() const {
    return data_[4];
}

uint32_t rav::rtcp_report_block_view::number_of_packets_lost() const {
    const std::array<uint8_t, 4> packets_lost {0, data_[5], data_[6], data_[7]};
    return byte_order::read_be<uint32_t>(packets_lost.data());
}

uint32_t rav::rtcp_report_block_view::extended_highest_sequence_number_received() const {
    return byte_order::read_be<uint32_t>(data_ + 8);
}

uint32_t rav::rtcp_report_block_view::inter_arrival_jitter() const {
    return byte_order::read_be<uint32_t>(data_ + 12);
}

rav::ntp::timestamp rav::rtcp_report_block_view::last_sr_timestamp() const {
    return ntp::timestamp::from_compact(byte_order::read_be<uint32_t>(data_ + 16));
}

uint32_t rav::rtcp_report_block_view::delay_since_last_sr() const {
    return byte_order::read_be<uint32_t>(data_ + 20);
}

const uint8_t* rav::rtcp_report_block_view::data() const {
    return data_;
}

size_t rav::rtcp_report_block_view::size() const {
    return size_bytes_;
}
