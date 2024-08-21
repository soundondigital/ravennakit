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

#include <cstddef>
#include <cstdint>

#include "Rtp.hpp"

namespace rav {

class RtcpReportBlockView {
  public:
    /**
     * Constructs an invalid report block;
     */
    RtcpReportBlockView() = default;

    /**
     * Constructs an RTCP report block view from the given data.
     * @param data The RTCP report block data.
     * @param data_length The length of the RTCP report block in bytes.
     */
    RtcpReportBlockView(const uint8_t* data, size_t data_length);

    /**
     * @returns True if this view points to data and has a size of > 0.
     */
    [[nodiscard]] bool is_valid() const;

    /**
     * @returns True if this report block appears to be correct, or false if not.
     */
    [[nodiscard]] rtp::VerificationResult verify() const;

  private:
    const uint8_t* data_ {};
    size_t data_length_ {0};
};

}  // namespace rav
