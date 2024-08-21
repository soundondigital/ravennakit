/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravenna-sdk/rtp/RtcpReportBlockView.h"

namespace {
    constexpr auto kReportBlockLength = 24;
}

rav::RtcpReportBlockView::RtcpReportBlockView(const uint8_t* data, const size_t data_length) :
    data_(data), data_length_(data_length) {}

bool rav::RtcpReportBlockView::is_valid() const {
    return data_ != nullptr && data_length_ > 0;
}

rav::rtp::VerificationResult rav::RtcpReportBlockView::verify() const {
    if (data_length_ < kReportBlockLength) {
        return rtp::VerificationResult::InvalidSenderInfoLength;
    }



    return rtp::VerificationResult::Ok;
}
