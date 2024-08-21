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

namespace rav::rtp {

constexpr auto kSsrcLength = 4;
constexpr auto kRtpTimestampLength = 4;

enum class VerificationResult {
    Ok,
    InvalidPointer,
    InvalidHeaderLength,
    InvalidSenderInfoLength,
    InvalidReportBlockLength,
    InvalidVersion,
};

}  // namespace rav::rtp
