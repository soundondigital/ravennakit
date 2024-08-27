/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/rtp/RtcpReportBlockView.h"

#include <array>

#include "ravennakit/platform/ByteOrder.hpp"

rav::RtcpReportBlockView::RtcpReportBlockView(const uint8_t* data, const size_t data_length) :
    data_(data), data_length_(data_length) {}

bool rav::RtcpReportBlockView::is_valid() const {
    return data_ != nullptr;
}

rav::rtp::Result rav::RtcpReportBlockView::validate() const {
    if (data_ == nullptr) {
        return rtp::Result::InvalidPointer;
    }

    if (data_length_ < kReportBlockLength) {
        return rtp::Result::InvalidReportBlockLength;
    }

    if (data_length_ > kReportBlockLength) {
        return rtp::Result::InvalidReportBlockLength;
    }

    return rtp::Result::Ok;
}

uint32_t rav::RtcpReportBlockView::ssrc() const {
    return byte_order::read_be<uint32_t>(data_);
}

uint8_t rav::RtcpReportBlockView::fraction_lost() const {
    return data_[4];
}

uint32_t rav::RtcpReportBlockView::number_of_packets_lost() const {
    const std::array<uint8_t, 4> packets_lost {0, data_[5], data_[6], data_[7]};
    return byte_order::read_be<uint32_t>(packets_lost.data());
}

uint32_t rav::RtcpReportBlockView::extended_highest_sequence_number_received() const {
    return byte_order::read_be<uint32_t>(data_ + 8);
}

uint32_t rav::RtcpReportBlockView::inter_arrival_jitter() const {
    return byte_order::read_be<uint32_t>(data_ + 12);
}

rav::ntp::Timestamp rav::RtcpReportBlockView::last_sr_timestamp() const {
    return ntp::Timestamp::from_compact(byte_order::read_be<uint32_t>(data_ + 16));
}

uint32_t rav::RtcpReportBlockView::delay_since_last_sr() const {
    return byte_order::read_be<uint32_t>(data_ + 20);
}

const uint8_t* rav::RtcpReportBlockView::data() const {
    return data_;
}

size_t rav::RtcpReportBlockView::data_length() const {
    return data_length_;
}
