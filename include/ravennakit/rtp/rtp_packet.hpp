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

#include "ravennakit/core/containers/byte_buffer.hpp"
#include "ravennakit/core/util/wrapping_uint.hpp"

namespace rav::rtp {

/**
 * This class holds state for an RTP packet and provides methods to encode it into a stream.
 */
class Packet {
  public:
    Packet() = default;

    /**
     * Sets the payload type.
     * @param value The payload type.
     */
    void payload_type(uint8_t value);

    /**
     * Sets the sequence number.
     * @param value The value to set.
     */
    void sequence_number(uint16_t value);

    /**
     * Increases the sequence number by the given value.
     * @param value The value to increment with.
     * @return The new sequence number.
     */
    wrapping_uint<uint16_t> sequence_number_inc(uint16_t value);

    /**
     * Sets the timestamp.
     * @param value The timestamp value.
     */
    void timestamp(uint32_t value);

    /**
     * Increases the timestamp by the given value.
     * @param value The value to add.
     * @return The new timestamp.
     */
    wrapping_uint<uint32_t> timestamp_inc(uint32_t value);

    /**
     * @return The timestamp.
     */
    [[nodiscard]] wrapping_uint<uint32_t> timestamp() const {
        return timestamp_;
    }

    /**
     * Sets the synchronization source identifier.
     * @param value The synchronization source identifier.
     */
    void ssrc(uint32_t value);

    /**
     * Encodes the RTP packet into given stream. This method writes to the stream as-is, the caller is responsible to
     * prepare the stream (reset it after previous calls).
     * @param payload_data The payload to encode.
     * @param payload_size The size of the payload in bytes.
     * @param buffer The buffer to write to.
     * @return true if the packet was successfully encoded and written, or false otherwise.
     */
    void encode(const uint8_t* payload_data, size_t payload_size, byte_buffer& buffer) const;

  private:
    uint8_t payload_type_ {0};
    wrapping_uint<uint16_t> sequence_number_ {0};
    wrapping_uint<uint32_t> timestamp_ {0};
    uint32_t ssrc_ {0};
};

}  // namespace rav
