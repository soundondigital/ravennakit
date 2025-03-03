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

#include <string>
#include <tuple>
#include "ravennakit/core/format.hpp"

namespace rav {

struct audio_format {
    enum class byte_order : uint8_t {
        le,
        be,
    };

    enum class channel_ordering : uint8_t {
        interleaved,
        noninterleaved,
    };

    byte_order byte_order {byte_order::le};
    audio_encoding encoding {};
    uint32_t sample_rate {};
    uint32_t num_channels {};
    channel_ordering ordering {channel_ordering::interleaved};

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

    static const char* to_string(const enum byte_order order) {
        return order == byte_order::le ? "le" : "be";
    }

    static const char* to_string(const channel_ordering order) {
        return order == channel_ordering::interleaved ? "interleaved" : "noninterleaved";
    }

    [[nodiscard]] bool is_valid() const {
        return encoding != audio_encoding::undefined && sample_rate != 0 && num_channels != 0;
    }

    [[nodiscard]] auto tie() const {
        return std::tie(encoding, sample_rate, num_channels, byte_order, ordering);
    }

    bool operator==(const audio_format& other) const {
        return tie() == other.tie();
    }

    bool operator!=(const audio_format& other) const {
        return tie() != other.tie();
    }
};

}  // namespace rav
