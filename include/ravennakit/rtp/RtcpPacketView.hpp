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

#include "RtcpReportBlockView.h"
#include "Rtp.hpp"
#include "ravennakit/containers/BufferView.hpp"
#include "ravennakit/ntp/Timestamp.hpp"

namespace rav {

class RtcpPacketView {
  public:
    enum class PacketType {
        /// Unknown packet type
        Unknown,
        /// Sender report, for transmission and reception statistics from participants that are active senders
        SenderReport,
        /// Receiver report, for reception statistics from participants that are not active senders and in combination
        /// with SR for active senders reporting on more than 31 sources
        ReceiverReport,
        /// Source description items, including CNAME
        SourceDescriptionItems,
        /// Indicates end of participation
        Bye,
        /// Application-specific functions
        App
    };

    RtcpPacketView() = default;

    /**
     * Constructs an RTCP packet view from the given data.
     * @param data The RTCP packet data.
     * @param data_length The length of the RTCP packet in bytes.
     */
    RtcpPacketView(const uint8_t* data, size_t data_length);

    /**
     * Validates the RTP header data. After this method returns all other methods should return valid data and not lead
     * to undefined behavior.
     * TODO: Add more validation checks.
     * @returns The result of the validation.
     */
    [[nodiscard]] rtp::Result validate() const;

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
    [[nodiscard]] PacketType packet_type() const;

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
    [[nodiscard]] RtcpReportBlockView get_report_block(size_t index) const;

    /**
     * @returns The profile specific extension data, or an empty buffer if no extension data is present.
     */
    [[nodiscard]] BufferView<const uint8_t> get_profile_specific_extension() const;

    /**
     * @return The next RTCP packet in the buffer, or an empty (invalid) view if no more packets are available.
     */
    [[nodiscard]] RtcpPacketView get_next_packet() const;

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

    /**
     * @returns A string representation of the RTCP header.
     */
    [[nodiscard]] std::string to_string() const;

    /**
     * @param packet_type The type to get a string representation for.
     * @return A string representation of given packet type.
     */
    static const char* packet_type_to_string(PacketType packet_type);

  private:
    const uint8_t* data_ {};
    size_t data_length_ {0};
};

}  // namespace rav
