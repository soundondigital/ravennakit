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

#include <cstdint>
#include <string>

#include "ravennakit/core/containers/buffer_view.hpp"

namespace rav::rtp {

/**
 * Functions for reading RTP header data. The data given is not copied or otherwise managed by this class so it's
 * cheap to create and use but make sure to keep the data alive while using this class.
 * RFC 3550 https://datatracker.ietf.org/doc/html/rfc3550
 */
class PacketView {
  public:
    /**
     * Constructs an RTP header from the given data.
     * @param data The RTP header data.
     * @param size_bytes The size of the RTP header data in bytes.
     */
    PacketView(const uint8_t* data, size_t size_bytes);

    /**
     * Validated the RTP header data. After this method returns all other methods should return valid data and not lead
     * to undefined behavior.
     * @returns True if the RTP header data is valid, or false if not.
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
     * @returns True if the extension bit is set.
     */
    [[nodiscard]] bool extension() const;

    /**
     * @returns The number of CSRC identifiers in the header.
     */
    [[nodiscard]] uint32_t csrc_count() const;

    /**
     * @returns True if the marker bit is set.
     */
    [[nodiscard]] bool marker_bit() const;

    /**
     * @returns The payload type.
     */
    [[nodiscard]] uint8_t payload_type() const;

    /**
     * @returns The sequence number.
     */
    [[nodiscard]] uint16_t sequence_number() const;

    /**
     * @returns The timestamp.
     */
    [[nodiscard]] uint32_t timestamp() const;

    /**
     * @return The synchronization source identifier.
     */
    [[nodiscard]] uint32_t ssrc() const;

    /**
     * Gets the CSRC identifier at the given index.
     * @param index The index of the CSRC identifier.
     * @returns The CSRC identifier, or 0 is the index or data is invalid.
     */
    [[nodiscard]] uint32_t csrc(uint32_t index) const;

    /**
     * @return Returns the header extension defined by profile data. Data is not endian swapped.
     */
    [[nodiscard]] uint16_t get_header_extension_defined_by_profile() const;

    /**
     * @return Returns the header extension data. Data is not endian swapped.
     */
    [[nodiscard]] buffer_view<const uint8_t> get_header_extension_data() const;

    /**
     * @returns Returns the length of the header which is also the start index of the payload data.
     */
    [[nodiscard]] size_t header_total_length() const;

    /**
     * @return Returns a view to the payload data.
     */
    [[nodiscard]] buffer_view<const uint8_t> payload_data() const;

    /**
     * @return Returns the size of the RTP header in bytes.
     */
    [[nodiscard]] size_t size() const;

    /**
     * @return Returns the data of the RTP packet.
     */
    [[nodiscard]] const uint8_t* data() const;

    /**
     * @returns A string representation of the RTP header.
     */
    [[nodiscard]] std::string to_string() const;

  private:
    const uint8_t* data_ {};
    size_t size_bytes_ {0};
};

}  // namespace rav
