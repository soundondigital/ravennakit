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

#include "ravenna-sdk/containers/BufferView.hpp"

#include <cstdint>
#include <string>

namespace rav {

/**
 * Functions for reading RTP header data. The data given is not copied or otherwise managed by this class so it's
 * cheap to create and use but make sure to keep the data alive while using this class.
 * RFC 3550 https://datatracker.ietf.org/doc/html/rfc3550
 */
class RtpHeaderView {
  public:
    enum class ValidationResult {
        Ok,
        InvalidPointer,
        InvalidHeaderLength,
        InvalidVersion,
    };

    /**
     * Constructs an RTP header from the given data.
     * @param data The RTP header data.
     * @param data_length The lenght of the RTP header data in bytes.
     */
    RtpHeaderView(const uint8_t* data, size_t data_length);

    /**
     * Validates the RTP header data.
     * @returns The result of the validation.
     */
    [[nodiscard]] ValidationResult validate() const;

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
    [[nodiscard]] BufferView<const uint8_t> get_header_extension_data() const;

    /**
     * @returns Returns the start index of the payload data.
     */
    [[nodiscard]] size_t header_total_length() const;

    /**
     * @return Returns a view to the payload data.
     */
    [[nodiscard]] BufferView<const uint8_t> payload_data() const;

    /**
     * @returns A string representation of the RTP header.
     */
    [[nodiscard]] std::string to_string() const;

  private:
    const uint8_t* data_ {};
    size_t data_length_ {0};
};

}  // namespace rsdk
