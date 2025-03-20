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
#include <string>

#include "rtcp_report_block_view.hpp"
#include "ravennakit/core/containers/buffer_view.hpp"
#include "ravennakit/ntp/ntp_timestamp.hpp"
#include "rtp_constants.hpp"

namespace rav::rtcp {

class rtcp_packet_view {
  public:
    enum class packet_type {
        /// Unknown packet type
        unknown,
        /// Sender report, for transmission and reception statistics from participants that are active senders
        sender_report_report,
        /// Receiver report, for reception statistics from participants that are not active senders and in combination
        /// with SR for active senders reporting on more than 31 sources
        receiver_report_report,
        /// Source description items, including CNAME
        source_description_items_items,
        /// Indicates end of participation
        bye,
        /// Application-specific functions
        app
    };

    rtcp_packet_view() = default;

    /**
     * Constructs an RTCP packet view from the given data.
     * @param data The RTCP packet data.
     * @param size_bytes The size of the RTCP packet in bytes.
     */
    rtcp_packet_view(const uint8_t* data, size_t size_bytes);

    /**
     * Validates the RTP header data. After this method returns all other methods should return valid data and not lead
     * to undefined behavior.
     * TODO: Add more validation checks.
     * @returns The result of the validation.
     */
    [[nodiscard]] bool validate() const;

    /**
     * @returns The version of the RTP header.
     */
    [[nodiscard]] uint8_t version() const;

    /**
     * @returns True if the padding bit is set.
     */
    [[nodiscard]] bool padding() const;

    /**
     * @returns The reception report count. Zero is a valid value.
     */
    [[nodiscard]] uint8_t reception_report_count() const;

    /**
     * @return The packet type
     */
    [[nodiscard]] packet_type type() const;

    /**
     * @returns The reported length of this RTCP packet in 32-bit words as encoded inside the data. While the length is
     * stored as n-1 in the data, this method returns the actual length (i.e. encoded length +1).
     */
    [[nodiscard]] uint16_t length() const;

    /**
     * @return The synchronization source identifier.
     */
    [[nodiscard]] uint32_t ssrc() const;

    /**
     * @return If the packet is a send report then this method returns the NTP timestamp, otherwise returns an empty
     * (0) timestamp.
     */
    [[nodiscard]] ntp::Timestamp ntp_timestamp() const;

    /**
     * @returns The RTP timestamp if this packet is a sender report, otherwise returns 0.
     */
    [[nodiscard]] uint32_t rtp_timestamp() const;

    /**
     * @returns The senders packet count, if packet type is sender report, otherwise returns 0.
     */
    [[nodiscard]] uint32_t packet_count() const;

    /**
     * @returns The senders octet count, if packet type is sender report, otherwise returns 0.
     */
    [[nodiscard]] uint32_t octet_count() const;

    /**
     * Fetches the report block for given index.
     * @param index The index of the report block to get.
     * @return The report block.
     */
    [[nodiscard]] rtcp_report_block_view get_report_block(size_t index) const;

    /**
     * @returns The profile specific extension data, or an empty buffer if no extension data is present.
     */
    [[nodiscard]] buffer_view<const uint8_t> get_profile_specific_extension() const;

    /**
     * @return The next RTCP packet in the buffer, or an empty (invalid) view if no more packets are available.
     */
    [[nodiscard]] rtcp_packet_view get_next_packet() const;

    /**
     * @returns The pointer to the data, or nullptr if not pointing to any data.
     */
    [[nodiscard]] const uint8_t* data() const;

    /**
     * @return The size of the data in bytes.
     */
    [[nodiscard]] size_t size() const;

    /**
     * @returns A string representation of the RTCP header.
     */
    [[nodiscard]] std::string to_string() const;

    /**
     * @param packet_type The type to get a string representation for.
     * @return A string representation of given packet type.
     */
    static const char* packet_type_to_string(packet_type packet_type);

  private:
    const uint8_t* data_ {};
    size_t size_bytes_ {0};
};

}  // namespace rav
