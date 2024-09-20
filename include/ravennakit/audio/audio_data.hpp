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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "ravennakit/core/assert.hpp"
#include "ravennakit/core/int24.hpp"
#include "ravennakit/platform/byte_order.hpp"

namespace rav::audio_data {

/**
 * Types for specifying interleaving.
 */
namespace interleaving {
    struct interleaved {};

    struct noninterleaved {};
}  // namespace interleaving

/**
 * Types for specifying byte order.
 */
namespace byte_order {
    struct le {
        static constexpr bool is_little_endian = true;

        /**
         * Reads a uint64_t value from a byte array.
         * @param data The data to read.
         * @return A uint64_t containing the read value, placed in the most significant bytes.
         */
        template<class T>
        static uint64_t read(const T* data) {
            uint64_t value {};
            std::memcpy(std::addressof(value), data, sizeof(T));
            if constexpr (big_endian) {
                value = rav::byte_order::swap_bytes(value);
            }
            return value;
        }

        /**
         * Writes given value into the data array.
         * @tparam T The value type.
         * @param data The data to write into.
         * @param size The size of the data. If smaller than the size of value, only the most significant bytes are
         * written.
         * @param value The value to write.
         */
        template<class DstType, class T>
        static void write(DstType* data, const size_t size, T value) {
            RAV_ASSERT(size <= sizeof(value));
            if constexpr (big_endian) {
                value = rav::byte_order::swap_bytes(value);
            }
            std::memcpy(data, std::addressof(value), size);
        }
    };

    struct be {
        static constexpr bool is_little_endian = false;

        /**
         * Reads a uint64_t value from a byte array.
         * @param data The data to read.
         * @return A uint64_t containing the read value, placed in the least significant bytes.
         */
        template<class T>
        static uint64_t read(const T* data) {
            uint64_t value {};
            if constexpr (little_endian) {
                std::memcpy(
                    reinterpret_cast<uint8_t*>(std::addressof(value)) + (sizeof(value) - sizeof(T)), data, sizeof(T)
                );
                value = rav::byte_order::swap_bytes(value);
            } else {
                std::memcpy(std::addressof(value), data, sizeof(T));
            }
            return value;
        }

        /**
         * Writes given value into the data array.
         * @tparam T The value type.
         * @param data The data to write into.
         * @param size The size of the data. If smaller than the size of value, only the most significant bytes are
         * written.
         * @param value The value to write.
         */
        template<class DstType, class T>
        static void write(DstType* data, const size_t size, T value) {
            RAV_ASSERT(size <= sizeof(value));
            if constexpr (little_endian) {
                value = rav::byte_order::swap_bytes(value);
                std::memcpy(data, reinterpret_cast<uint8_t*>(std::addressof(value)) + (sizeof(value) - size), size);
            } else {
                std::memcpy(std::addressof(value), data, size);
            }
        }
    };

    struct ne {
        static constexpr bool is_little_endian = little_endian;

        /**
         * Reads a uint64_t value from a byte array.
         * @param data The data to read.
         * @return A uint64_t containing the read value, placed in the least significant bytes.
         */
        template<class T>
        static uint64_t read(const T* data) {
            uint64_t value {};
            if constexpr (little_endian) {
                std::memcpy(std::addressof(value), data, sizeof(T));
            } else {
                RAV_ASSERT_FALSE("Implement me");
            }
            return value;
        }

        /**
         * Writes given value into the data array.
         * @tparam T The value type.
         * @param data The data to write into.
         * @param size The size of the data. If smaller than the size of value, only the most significant bytes are
         * written.
         * @param value The value to write.
         */
        template<class DstType, class T>
        static void write(DstType* data, const size_t size, T value) {
            RAV_ASSERT(size <= sizeof(value));
            if constexpr (little_endian) {
                std::memcpy(data, std::addressof(value), size);
            } else {
                RAV_ASSERT_FALSE("Implement me");
            }
        }
    };
}  // namespace byte_order

/**
 * Converts a sample from one format to another.
 * @tparam SrcType The source sample format. One of the structs in the format namespace.
 * @tparam SrcByteOrder The source byte order. One of the structs in the byte_order namespace.
 * @tparam DstType The destination sample format. One of the structs in the format namespace.
 * @tparam DstByteOrder The destination byte order. One of the structs in the byte_order namespace.
 * @param src The source data.
 * @param dst The destination data.
 */
template<class SrcType, class SrcByteOrder, class DstType, class DstByteOrder>
static void convert_sample(const SrcType* src, DstType* dst) {
    if constexpr (std::is_same_v<SrcType, DstType> && std::is_same_v<SrcByteOrder, DstByteOrder>) {
        std::memcpy(dst, src, sizeof(SrcType));
    } else if constexpr (std::is_same_v<SrcType, DstType>) {
        // Only byte order differs
        DstByteOrder::write(dst, sizeof(DstType), SrcByteOrder::read(src));
    } else {
        const auto src_sample = SrcByteOrder::read(src);

        if constexpr (std::is_same_v<SrcType, uint8_t>) {
            if constexpr (std::is_same_v<DstType, int8_t>) {
                DstByteOrder::write(dst, sizeof(DstType), src_sample - 0x80);
            } else {
                RAV_ASSERT_FALSE("Conversion not available");
            }
        } else if constexpr (std::is_same_v<SrcType, int8_t>) {
            if constexpr (std::is_same_v<DstType, int16_t>) {
                DstByteOrder::write(dst, sizeof(DstType), src_sample << 8);
            } else {
                RAV_ASSERT_FALSE("Conversion not available");
            }
        } else if constexpr (std::is_same_v<SrcType, int16_t>) {
            if constexpr (std::is_same_v<DstType, int24_t>) {
                DstByteOrder::write(dst, sizeof(DstType), src_sample << 8);
            } else if constexpr (std::is_same_v<DstType, int32_t>) {
                DstByteOrder::write(dst, sizeof(DstType), src_sample << 16);
            } else if constexpr (std::is_same_v<DstType, float>) {
                const bool is_negative = static_cast<int64_t>(src_sample) << 48 < 0;
                auto f = static_cast<float>(src_sample) * 0.000030517578125f * (is_negative ? -1 : 1);
                DstByteOrder::write(dst, sizeof(DstType), f);
            } else if constexpr (std::is_same_v<DstType, double>) {
                const bool is_negative = static_cast<int64_t>(src_sample) << 48 < 0;
                auto f = static_cast<double>(src_sample) * 0.000030517578125f * (is_negative ? -1 : 1);
                DstByteOrder::write(dst, sizeof(DstType), f);
            } else {
                RAV_ASSERT_FALSE("Conversion not available");
            }
        } else if constexpr (std::is_same_v<SrcType, int24_t>) {
            if constexpr (std::is_same_v<DstType, float>) {
                const bool is_negative = static_cast<int64_t>(src_sample) << 40 < 0;
                auto f = static_cast<float>(src_sample) * 0.00000011920928955078125f * (is_negative ? -1 : 1);
                DstByteOrder::write(dst, sizeof(DstType), f);
            } else if constexpr (std::is_same_v<DstType, double>) {
                const bool is_negative = static_cast<int64_t>(src_sample) << 40 < 0;
                auto f = static_cast<double>(src_sample) * 0.00000011920928955078125f * (is_negative ? -1 : 1);
                DstByteOrder::write(dst, sizeof(DstType), f);
            } else {
                RAV_ASSERT_FALSE("Conversion not available");
            }
        } else if constexpr (std::is_same_v<SrcType, float>) {
            if constexpr (std::is_same_v<DstType, int16_t>) {
                const auto scaled = *reinterpret_cast<const float*>(&src_sample) * 32767.0f;
                DstByteOrder::write(dst, sizeof(DstType), static_cast<int16_t>(scaled));
            } else if constexpr (std::is_same_v<DstType, int24_t>) {
                const auto scaled = *reinterpret_cast<const float*>(&src_sample) * 8388607.f;
                DstByteOrder::write(dst, sizeof(DstType), static_cast<int24_t>(scaled));
            } else {
                RAV_ASSERT_FALSE("Conversion not available");
            }
        } else if constexpr (std::is_same_v<SrcType, double>) {
            RAV_ASSERT_FALSE("Conversion not available");
        } else {
            RAV_ASSERT_FALSE("Conversion not available");
        }
    }
}

/**
 * Converts audio data from one format to another and converts interleaving and byte order.
 * @tparam SrcType The source sample format.
 * @tparam SrcByteOrder The source byte order. One of the structs in the byte_order namespace.
 * @tparam SrcInterleaving The source interleaving. One of the structs in the interleaving namespace.
 * @tparam DstType The destination sample format.
 * @tparam DstByteOrder The destination byte order. One of the structs in the byte_order namespace.
 * @tparam DstInterleaving The destination interleaving. One of the structs in the interleaving namespace.
 * @param src The source data.
 * @param src_size The size of the source data.
 * @param dst The destination data.
 * @param dst_size The size of the destination data.
 * @param num_channels The number of channels in the audio data.
 * @return True if the conversion was successful, false otherwise.
 */
template<
    class SrcType, class SrcByteOrder, class SrcInterleaving, class DstType, class DstByteOrder, class DstInterleaving>
static bool
convert(const SrcType* src, const size_t src_size, DstType* dst, const size_t dst_size, const size_t num_channels) {
    const auto src_size_bytes = src_size * sizeof(SrcType);
    const auto dst_size_bytes = dst_size * sizeof(DstType);

    // Shortcut for when no conversion is needed
    if constexpr (std::is_same_v<SrcType, DstType> && std::is_same_v<SrcByteOrder, DstByteOrder> && std::is_same_v<SrcInterleaving, DstInterleaving>) {
        if (src_size == 0 || src_size != dst_size) {
            return false;
        }
        std::copy_n(src, src_size, dst);
        return true;
    } else if (std::is_same_v<SrcType, DstType> && std::is_same_v<SrcInterleaving, DstInterleaving>) {
        RAV_ASSERT(src_size_bytes == dst_size_bytes);

        std::memcpy(dst, src, src_size_bytes);

        if constexpr (SrcByteOrder::is_little_endian == DstByteOrder::is_little_endian) {
            return true;  // No need for swapping
        }

        for (size_t i = 0; i < dst_size; ++i) {
            dst[i] = rav::byte_order::swap_bytes(dst[i]);
        }

        return true;
    }

    const auto num_frames = src_size / num_channels;

    if (num_frames != dst_size / num_channels) {
        return false;  // Unequal amount of frames between src and dst
    }

    if constexpr (std::is_same_v<SrcInterleaving, interleaving::interleaved>) {
        if constexpr (std::is_same_v<DstInterleaving, interleaving::interleaved>) {
            // Interleaved src, interleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                convert_sample<SrcType, SrcByteOrder, DstType, DstByteOrder>(src + i, dst + i);
            }
        } else {
            // Interleaved src, noninterleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i % num_channels;
                const auto dst_i = ch * num_frames + i / num_channels;
                convert_sample<SrcType, SrcByteOrder, DstType, DstByteOrder>(src + i, dst + dst_i);
            }
        }
    } else if constexpr (std::is_same_v<SrcInterleaving, interleaving::noninterleaved>) {
        if constexpr (std::is_same_v<DstInterleaving, interleaving::interleaved>) {
            // Noninterleaved src, interleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i % num_frames;
                const auto src_i = ch * num_frames + i / num_channels;
                convert_sample<SrcType, SrcByteOrder, DstType, DstByteOrder>(src + src_i, dst + i);
            }
        } else {
            // Noninterleaved src, noninterleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                convert_sample<SrcType, SrcByteOrder, DstType, DstByteOrder>(src + i, dst + i);
            }
        }
    } else {
        RAV_ASSERT_FALSE("Invalid interleaving");
        return false;
    }

    return true;
}

/**
 * Converts audio data from one format to another and converts interleaving and byte order.
 * @tparam SrcType The source sample format.
 * @tparam SrcByteOrder The source byte order. One of the structs in the byte_order namespace.
 * @tparam SrcInterleaving The source interleaving. One of the structs in the interleaving namespace.
 * @tparam DstType The destination sample format.
 * @tparam DstByteOrder The destination byte order. One of the structs in the byte_order namespace.
 * @param src The source data.
 * @param src_size The size of the source data.
 * @param num_channels The number of channels in the audio data.
 * @param dst The destination data.
 * @param dst_start_frame The starting frame in the destination data.
 * @return True if the conversion was successful, false otherwise.
 */
template<class SrcType, class SrcByteOrder, class SrcInterleaving, class DstType, class DstByteOrder>
static bool convert(
    const SrcType* src, const size_t src_size, const size_t num_channels, DstType* const* dst,
    const size_t dst_start_frame = 0
) {
    const auto num_frames = src_size / num_channels;
    if (num_frames == 0) {
        return false;
    }

    if constexpr (std::is_same_v<SrcInterleaving, interleaving::interleaved>) {
        // interleaved to non-interleaved
        for (size_t i = 0; i < num_frames * num_channels; ++i) {
            const auto ch = i % num_channels;
            const auto frame = i / num_channels;
            convert_sample<SrcType, SrcByteOrder, DstType, DstByteOrder>(src + i, dst[ch] + frame + dst_start_frame);
        }
    } else if constexpr (std::is_same_v<SrcInterleaving, interleaving::noninterleaved>) {
        // interleaved to interleaved (channels)
        for (size_t i = 0; i < num_frames * num_channels; ++i) {
            const auto ch = i / num_frames;
            const auto frame = i % num_frames;
            convert_sample<SrcType, SrcByteOrder, DstType, DstByteOrder>(src + i, dst[ch] + frame + dst_start_frame);
        }
    } else {
        RAV_ASSERT_FALSE("Invalid interleaving");
        return false;
    }

    return true;
}

/**
 * Converts audio data from one format to another and converts interleaving and byte order.
 * @tparam SrcType The source sample format.
 * @tparam SrcByteOrder The source byte order. One of the structs in the byte_order namespace.
 * @tparam DstType The destination sample format.
 * @tparam DstByteOrder The destination byte order. One of the structs in the byte_order namespace.
 * @tparam DstInterleaving The destination interleaving. One of the structs in the interleaving namespace.
 * @param src The source data.
 * @param num_frames The number of frames in the audio data.
 * @param num_channels The number of channels in the audio data.
 * @param src_start_frame The starting frame in the source data.
 * @param dst The destination data.
 * @return True if the conversion was successful, false otherwise.
 */
template<class SrcType, class SrcByteOrder, class DstType, class DstByteOrder, class DstInterleaving>
static bool convert(
    const SrcType* const* src, const size_t num_frames, const size_t num_channels, const size_t src_start_frame,
    DstType* dst
) {
    if constexpr (std::is_same_v<DstInterleaving, interleaving::interleaved>) {
        for (size_t frame = 0; frame < num_frames; ++frame) {
            for (size_t ch = 0; ch < num_channels; ++ch) {
                convert_sample<SrcType, SrcByteOrder, DstType, DstByteOrder>(
                    src[ch] + frame + src_start_frame, dst + frame * num_channels + ch
                );
            }
        }
    } else if constexpr (std::is_same_v<DstInterleaving, interleaving::noninterleaved>) {
        for (size_t frame = 0; frame < num_frames; ++frame) {
            for (size_t ch = 0; ch < num_channels; ++ch) {
                convert_sample<SrcType, SrcByteOrder, DstType, DstByteOrder>(
                    src[ch] + frame + src_start_frame, dst + num_frames * ch + frame
                );
            }
        }
    } else {
        RAV_ASSERT_FALSE("Invalid interleaving");
        return false;
    }

    return true;
}

}  // namespace rav::audio_data
