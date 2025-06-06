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
#include "ravennakit/core/format.hpp"
#include "ravennakit/core/json.hpp"

#include <tuple>
#include <string>

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

    static const char* to_string(const ByteOrder order) {
        return order == ByteOrder::le ? "le" : "be";
    }

    static const char* to_string(const ChannelOrdering order) {
        return order == ChannelOrdering::interleaved ? "interleaved" : "noninterleaved";
    }

    static std::optional<ByteOrder> byte_order_from_string(const std::string& str) {
        if (str == "le") {
            return ByteOrder::le;
        }
        if (str == "be") {
            return ByteOrder::be;
        }
        return std::nullopt;
    }

    static std::optional<ChannelOrdering> channel_ordering_from_string(const std::string& str) {
        if (str == "interleaved") {
            return ChannelOrdering::interleaved;
        }
        if (str == "noninterleaved") {
            return ChannelOrdering::noninterleaved;
        }
        return std::nullopt;
    }

    /**
     * @param order The byte order to set.
     * @return A copy of this AudioFormat with the byte order set to the given value.
     */
    [[nodiscard]] AudioFormat with_byte_order(const ByteOrder order) const {
        return {order, encoding, ordering, sample_rate, num_channels};
    }

#if RAV_HAS_NLOHMANN_JSON

    /**
     * @return A JSON object representing this AudioFormat.
     */
    [[nodiscard]] nlohmann::json to_json() const {
        return nlohmann::json {
            {"encoding", audio_encoding_to_string(encoding)},
            {"sample_rate", sample_rate},
            {"num_channels", num_channels},
            {"byte_order", to_string(byte_order)},
            {"channel_ordering", to_string(ordering)}
        };
    }

    /**
     * Converts a JSON object to an AudioFormat object.
     * @param json The JSON object to convert.
     * @return An AudioFormat object, or an error message if the conversion fails.
     */
    static tl::expected<AudioFormat, std::string> from_json(nlohmann::json json) {
        try {
            const auto encoding = audio_encoding_from_string(json.at("encoding").get<std::string>().c_str());
            if (!encoding) {
                return tl::unexpected(
                    fmt::format("Invalid audio encoding: {}", json.at("encoding").get<std::string>())
                );
            }

            const auto byte_order = byte_order_from_string(json.at("byte_order").get<std::string>());
            if (!byte_order) {
                return tl::unexpected(fmt::format("Invalid byte order: {}", json.at("byte_order").get<std::string>()));
            }

            const auto channel_ordering = channel_ordering_from_string(json.at("channel_ordering").get<std::string>());
            if (!channel_ordering) {
                return tl::unexpected(
                    fmt::format("Invalid channel ordering: {}", json.at("channel_ordering").get<std::string>())
                );
            }

            AudioFormat format;
            format.encoding = *encoding;
            format.sample_rate = json.at("sample_rate").get<uint32_t>();
            format.num_channels = json.at("num_channels").get<uint32_t>();
            format.byte_order = *byte_order;
            format.ordering = *channel_ordering;
            return format;
        } catch (const std::exception& e) {
            return tl::unexpected(fmt::format("Failed to parse AudioFormat JSON: {}", e.what()));
        }
    }

#endif
};

}  // namespace rav
