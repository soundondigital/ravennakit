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

#include "ravennakit/ntp/ntp_timestamp.hpp"

namespace rav::rtcp {

class ReportBlockView {
  public:
    static constexpr auto k_report_block_length_length = 24;

    /**
     * Constructs an invalid report block;
     */
    ReportBlockView() = default;

    /**
     * Constructs an RTCP report block view from the given data.
     * @param data The RTCP report block data.
     * @param size_bytes The size of the RTCP report block in bytes.
     */
    ReportBlockView(const uint8_t* data, size_t size_bytes);

    /**
     * @returns True if this report block appears to be correct, or false if not.
     */
    [[nodiscard]] bool validate() const;

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
     * @returns The pointer to the data, or nullptr if not pointing to any data.
     */
    [[nodiscard]] const uint8_t* data() const;

    /**
     * @return The size of the data in bytes.
     */
    [[nodiscard]] size_t size() const;

  private:
    const uint8_t* data_ {};
    size_t size_bytes_ {0};
};

}  // namespace rav::rtcp
