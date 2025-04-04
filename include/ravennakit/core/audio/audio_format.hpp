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

#include "audio_encoding.hpp"
#include "ravennakit/core/byte_order.hpp"

#include <string>
#include <tuple>
#include "ravennakit/core/format.hpp"

namespace rav {

struct AudioFormat {
    enum class ByteOrder : uint8_t {
        le,
        be,
    };

    enum class ChannelOrdering : uint8_t {
        interleaved,
        noninterleaved,
    };

    ByteOrder byte_order {little_endian ? ByteOrder::le : ByteOrder::be};
    AudioEncoding encoding {};
    ChannelOrdering ordering {ChannelOrdering::interleaved};
    uint32_t sample_rate {};
    uint32_t num_channels {};

    [[nodiscard]] uint8_t bytes_per_sample() const {
        return audio_encoding_bytes_per_sample(encoding);
    }

    [[nodiscard]] uint32_t bytes_per_frame() const {
        return bytes_per_sample() * num_channels;
    }

    [[nodiscard]] uint8_t ground_value() const {
        return audio_encoding_ground_value(encoding);
    }

    [[nodiscard]] std::string to_string() const {
        return fmt::format(
            "{}/{}/{}/{}/{}", audio_encoding_to_string(encoding), sample_rate, num_channels, to_string(ordering),
            to_string(byte_order)
        );
    }

    [[nodiscard]] bool is_valid() const {
        return encoding != AudioEncoding::undefined && sample_rate != 0 && num_channels != 0;
    }

    [[nodiscard]] auto tie() const {
        return std::tie(encoding, sample_rate, num_channels, byte_order, ordering);
    }

    bool operator==(const AudioFormat& other) const {
        return tie() == other.tie();
    }

    bool operator!=(const AudioFormat& other) const {
        return tie() != other.tie();
    }

    [[nodiscard]] bool is_native_byte_order() const {
        return little_endian == (byte_order == ByteOrder::le);
    }

    static const char* to_string(const enum ByteOrder order) {
        return order == ByteOrder::le ? "le" : "be";
    }

    static const char* to_string(const ChannelOrdering order) {
        return order == ChannelOrdering::interleaved ? "interleaved" : "noninterleaved";
    }

    /**
     * @param order The byte order to set.
     * @return A copy of this AudioFormat with the byte order set to the given value.
     */
    [[nodiscard]] AudioFormat with_byte_order(const ByteOrder order) const {
        return {order, encoding, ordering, sample_rate, num_channels};
    }
};

}  // namespace rav
