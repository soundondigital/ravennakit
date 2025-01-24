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
    enum class byte_order {
        le,
        be,
    };

    audio_encoding encoding {};
    uint32_t sample_rate {};
    uint32_t num_channels {};
    byte_order byte_order {byte_order::le};

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
        return fmt::format("{}/{}/{}", audio_encoding_to_string(encoding), sample_rate, num_channels);
    }

    [[nodiscard]] bool is_valid() const {
        return encoding != audio_encoding::undefined && sample_rate != 0 && num_channels != 0;
    }

    bool operator==(const audio_format& other) const {
        return std::tie(encoding, sample_rate, num_channels, byte_order)
            == std::tie(other.encoding, other.sample_rate, other.num_channels, byte_order);
    }

    bool operator!=(const audio_format& rhs) const {
        return !(*this == rhs);
    }
};

}  // namespace rav
