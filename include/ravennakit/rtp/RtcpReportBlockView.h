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
#include "ravennakit/ntp/Timestamp.hpp"

namespace rav {

class RtcpReportBlockView {
  public:
    static constexpr auto kReportBlockLength = 24;

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
     * @returns True if this report block appears to be correct, or false if not.
     */
    [[nodiscard]] rtp::Result validate() const;

    /**
     * @returns The SSRC of the sender of the RTCP report block.
     */
    [[nodiscard]] uint32_t ssrc() const;

    /**
     * @returns The fraction of packets lost.
     */
    [[nodiscard]] uint8_t fraction_lost() const;

    /**
     * @returns The cumulative number of packets lost.
     */
    [[nodiscard]] uint32_t number_of_packets_lost() const;

    /**
     * @returns The extended highest sequence number received.
     */
    [[nodiscard]] uint32_t extended_highest_sequence_number_received() const;

    /**
     * @returns The inter-arrival jitter.
     */
    [[nodiscard]] uint32_t inter_arrival_jitter() const;

    /**
     * @return The last SR timestamp.
     */
    [[nodiscard]] ntp::Timestamp last_sr_timestamp() const;

    /**
     * @return The delay since the last SR.
     */
    [[nodiscard]] uint32_t delay_since_last_sr() const;

    /**
     * Checks if this view references data. Note that this method does not validate the data itself; use validate() for
     * data validation.
     * @returns True if this view points to data.
     */
    [[nodiscard]] bool is_valid() const;

    /**
     * @returns The pointer to the data, or nullptr if not pointing to any data.
     */
    [[nodiscard]] const uint8_t* data() const;

    /**
     * @return The length of the data in bytes.
     */
    [[nodiscard]] size_t data_length() const;

  private:
    const uint8_t* data_ {};
    size_t data_length_ {0};
};

}  // namespace rav
