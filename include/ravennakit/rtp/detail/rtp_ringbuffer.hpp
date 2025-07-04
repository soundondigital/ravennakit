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

#include "ravennakit/core/containers/fifo_buffer.hpp"
#include "ravennakit/rtp/rtp_packet_view.hpp"
#include "ravennakit/core/log.hpp"
#include "ravennakit/core/util/wrapping_uint.hpp"

namespace rav::rtp {

/**
 * A buffer which operates on bytes, unaware of its contents. Can be used to account for jitter when receiving RTP data.
 * This class has no notion of a start timestamp, or delay value. It is up to the user to prevent overwriting newer
 * packets with older packets. The reason for this is to allow different readers with different delay settings to use
 * the same buffer.
 */
class Ringbuffer {
  public:
    explicit Ringbuffer() = default;

    /**
     * Resizes the buffer.
     * @param buffer_capacity_frames The total capacity of the buffer in frames.
     * @param bytes_per_frame The number of bytes per frame.
     */
    void resize(const uint32_t buffer_capacity_frames, const uint32_t bytes_per_frame) {
        const auto new_capacity = buffer_capacity_frames * bytes_per_frame;
        if (new_capacity == buffer_.size() && bytes_per_frame == bytes_per_frame_) {
            return;  // Nothing to do here
        }
        bytes_per_frame_ = bytes_per_frame;
        buffer_.resize(buffer_capacity_frames * bytes_per_frame_, ground_value_);
        std::fill(buffer_.begin(), buffer_.end(), ground_value_);
    }

    /**
     * Writes data to the buffer. Older packets can be written as well, but make sure packet are not too old, otherwise
     * they might overwrite newer packets (as a result of circular buffering).
     * @param at_timestamp Places the data at this timestamp.
     * @param payload The data to write to the buffer.
     * @return true if the data was written, false if the payload is not a multiple of bytes_per_frame_ or if the
     * payload is larger than the buffer size.
     */
    bool write(const uint32_t at_timestamp, const BufferView<const uint8_t>& payload) {
        RAV_ASSERT(payload.data() != nullptr, "Payload data must not be nullptr.");
        RAV_ASSERT(payload.size_bytes() > 0, "Payload size must be greater than 0.");

        if (payload.size_bytes() % bytes_per_frame_ != 0) {
            RAV_ERROR("Payload size must be a multiple of bytes per frame.");
            return false;
        }

        if (payload.size_bytes() > buffer_.size()) {
            RAV_ERROR("Payload size is larger than the buffer size.");
            return false;
        }

        const Fifo::Position position(
            static_cast<size_t>(at_timestamp) * bytes_per_frame_, buffer_.size(), payload.size()
        );

        std::memcpy(buffer_.data() + position.index1, payload.data(), position.size1);

        if (position.size2 > 0) {
            std::memcpy(buffer_.data(), payload.data() + position.size1, position.size2);
        }

        const auto end_ts =
            WrappingUint32(at_timestamp) + static_cast<uint32_t>(payload.size_bytes() / bytes_per_frame_);

        if (end_ts > next_ts_) {
            next_ts_ = end_ts;
        }

        return true;
    }

    /**
     * Reads data from the buffer.
     * Once the data is read, the buffer is cleared (i.e. the data is set to the ground value).
     * @param at_timestamp The timestamp to read from.
     * @param buffer The destination to write the data to.
     * @param buffer_size The size of the buffer in bytes.
     * @param clear_data If true, the data in the buffer will be cleared after reading.
     * @returns true if buffer_size bytes were read, or false if buffer_size bytes couldn't be read.
     */
    bool read(const uint32_t at_timestamp, uint8_t* buffer, const size_t buffer_size, const bool clear_data = false) {
        RAV_ASSERT(buffer != nullptr, "Buffer must not be nullptr.");
        RAV_ASSERT(buffer_size > 0, "Buffer size must be greater than 0.");

        if (buffer_size % bytes_per_frame_ != 0) {
            RAV_WARNING("Buffer size must be a multiple of bytes per frame.");
            return false;
        }

        if (buffer_size > buffer_.size()) {
            RAV_WARNING("Buffer size is larger than the buffer size.");
            return false;
        }

        const Fifo::Position position(
            static_cast<size_t>(at_timestamp) * bytes_per_frame_, buffer_.size(), buffer_size
        );

        std::memcpy(buffer, buffer_.data() + position.index1, position.size1);

        if (clear_data) {
            std::fill_n(buffer_.data() + position.index1, position.size1, ground_value_);
        }

        if (position.size2 > 0) {
            std::memcpy(buffer + position.size1, buffer_.data(), position.size2);
            if (clear_data) {
                std::fill_n(buffer_.data(), position.size2, ground_value_);
            }
        }

        return true;
    }

    /**
     * Fills the buffer with a value until (but not including) the given timestamp. If given timestamp is older than the
     * existing data nothing will happen - i.e. an older packet will not overwrite a newer packet.
     * @param at_timestamp The timestamp to fill until.
     * @returns true if any data was cleared, false if no data was cleared.
     */
    bool clear_until(const uint32_t at_timestamp) {
        if (next_ts_ >= WrappingUint32(at_timestamp)) {
            return false;  // Nothing to do here
        }

        const auto number_of_elements = (WrappingUint32(at_timestamp) - next_ts_.value()).value() * bytes_per_frame_;

        const Fifo::Position position(
            next_ts_.value() * bytes_per_frame_, buffer_.size(),
            std::min(static_cast<size_t>(number_of_elements), buffer_.size())
        );

        std::memset(buffer_.data() + position.index1, ground_value_, position.size1);

        if (position.size2 > 0) {
            std::memset(buffer_.data(), ground_value_, position.size2);
        }

        next_ts_ = at_timestamp;
        return true;
    }

    /**
     * @returns the timestamp following the most recent data (packet start ts + packet size).
     */
    [[nodiscard]] WrappingUint32 get_next_ts() const {
        return next_ts_;
    }

    /**
     * Sets the next timestamp to the given value.
     * @param next_ts The next timestamp.
     */
    void set_next_ts(const uint32_t next_ts) {
        next_ts_ = next_ts;
    }

    /**
     * Sets the value to clear the buffer with. For example, 0x0 for signed audio samples, 0x80 for unsigned 8-bit
     * samples.
     * @param ground_value The value to clear the buffer with.
     */
    void set_ground_value(const uint8_t ground_value) {
        ground_value_ = ground_value;
    }

  private:
    uint32_t bytes_per_frame_ = 0;  // Number of bytes (octets) per frame
    WrappingUint32 next_ts_;        // Producer ts
    std::vector<uint8_t> buffer_;   // Stores the actual data
    uint8_t ground_value_ = 0;      // Value to clear the buffer with.
};

}  // namespace rav::rtp
