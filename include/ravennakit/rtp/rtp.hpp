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

#include <ravennakit/core/platform.hpp>

namespace rav::rtp {

using ssrc = uint32_t;

constexpr auto kSsrcLength = 4;
constexpr auto kRtpTimestampLength = 4;

enum class validation_result {
    invalid_pointer,
    invalid_header_length_length,
    invalid_sender_info_length_length,
    invalid_report_block_length_length,
    invalid_version_version,
    invalid_state,
    resource_failure,
    multicast_membership_failure,
};

}  // namespace rav::rtp
