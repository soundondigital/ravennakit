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
#include "ravennakit/platform/byte_order.hpp"

namespace rav::audio_data {
namespace interleaving {
    struct interleaved {};

    struct noninterleaved {};
}  // namespace interleaving

namespace byte_order {
    struct le {
        template<class T>
        static T swap(const T value) {
#if RAV_LITTLE_ENDIAN
            return value;
#else
            return rav::byte_order::swap_bytes(value);
#endif
        }
    };

    struct be {
        template<class T>
        static T swap(const T value) {
#if RAV_LITTLE_ENDIAN
            return rav::byte_order::swap_bytes(value);
#else
            return value;
#endif
        }
    };

    struct ne {
        template<class T>
        static T swap(const T value) {
            return value;
        }
    };
}  // namespace byte_order

namespace format {
    struct int8 {
        static constexpr size_t sample_size = 1;
        using type = int8_t;
    };

    struct int16 {
        static constexpr size_t sample_size = 2;
        using type = int16_t;
    };

    struct int24 {
        static constexpr size_t sample_size = 3;
        using type = int32_t;
    };

    struct int24in32 {
        static constexpr size_t sample_size = 4;
        using type = int32_t;
    };

    struct int32 {
        static constexpr size_t sample_size = 4;
        using type = int32_t;
    };

    struct int64 {
        static constexpr size_t sample_size = 8;
        using type = int64_t;
    };

    struct uint8 {
        static constexpr size_t sample_size = 1;
        using type = uint8_t;
    };

    struct uint16 {
        static constexpr size_t sample_size = 2;
        using type = uint16_t;
    };

    struct uint24 {
        static constexpr size_t sample_size = 3;
        using type = uint32_t;
    };

    struct uint24in32 {
        static constexpr size_t sample_size = 4;
        using type = uint32_t;
    };

    struct uint32 {
        static constexpr size_t sample_size = 4;
        using type = uint32_t;
    };

    struct uint64 {
        static constexpr size_t sample_size = 8;
        using type = uint64_t;
    };

    struct f32 {
        static constexpr size_t sample_size = 4;
        using type = float;
    };

    struct f64 {
        static constexpr size_t sample_size = 8;
        using type = double;
    };
}  // namespace format

template<class Format, class ByteOrder>
typename Format::type read_sample(const uint8_t* data) {
    static_assert(
        Format::sample_size <= sizeof(typename Format::type), "sample_size is larger than the size of the value"
    );
    typename Format::type value;
    std::memcpy(std::addressof(value), data, Format::sample_size);
    return ByteOrder::swap(value);
}

template<class Format, class ByteOrder>
void write_sample(uint8_t* data, typename Format::type value) {
    static_assert(
        Format::sample_size <= sizeof(typename Format::type), "sample_size is larger than the size of the value"
    );
    value = ByteOrder::swap(value);
    std::memcpy(data, std::addressof(value), Format::sample_size);
}

template<class SrcFormat, class DstFormat>
static typename DstFormat::type convert_sample_value(typename SrcFormat::type src) {
    if constexpr (std::is_same_v<SrcFormat, DstFormat>) {
        return src;
    } else if (sizeof(typename SrcFormat::type) <= sizeof(typename DstFormat::type)) {
        if constexpr ((std::is_signed_v<typename SrcFormat::type> && std::is_signed_v<typename DstFormat::type>) || (!std::is_signed_v<typename SrcFormat::type> && !std::is_signed_v<typename DstFormat::type>)) {
            return src;
        }
    } else if (sizeof(typename SrcFormat::type) == sizeof(typename DstFormat::type)) {
        return static_cast<typename DstFormat::type>(src);
    }

    RAV_ASSERT_FALSE("Not implemented");
}

template<class SrcFormat, class SrcByteOrder, class DstFormat, class DstByteOrder>
static void convert_sample(const uint8_t* src, uint8_t* dst) {
    if constexpr (std::is_same_v<SrcFormat, DstFormat> && std::is_same_v<SrcByteOrder, DstByteOrder>) {
        std::memcpy(dst, src, SrcFormat::sample_size);
    } else if (std::is_same_v<SrcFormat, DstFormat>) {
        // Only byte order differs
        write_sample<DstFormat, DstByteOrder>(dst, read_sample<SrcFormat, SrcByteOrder>(src));
    } else {
        const auto src_sample = read_sample<SrcFormat, SrcByteOrder>(src);
        auto dst_sample = convert_sample_value<SrcFormat, DstFormat>(src_sample);
        write_sample<DstFormat, DstByteOrder>(dst, dst_sample);
    }
}

template<
    class SrcFormat, class SrcByteOrder, class SrcInterleaving, class DstFormat, class DstByteOrder,
    class DstInterleaving>
static bool
convert(const uint8_t* src, const size_t src_size, uint8_t* dst, const size_t dst_size, const size_t num_channels) {
    if (src_size % SrcFormat::sample_size != 0) {
        return false;  // src_size is not a multiple of sample size
    }

    if (dst_size % DstFormat::sample_size != 0) {
        return false;  // dst_size is not a multiple of sample size
    }

    // Shortcut for when no conversion is needed
    if constexpr (std::is_same_v<SrcFormat, DstFormat> && std::is_same_v<SrcByteOrder, DstByteOrder> && std::is_same_v<SrcInterleaving, DstInterleaving>) {
        if (src_size == 0 || src_size != dst_size) {
            return false;
        }
        std::copy_n(src, src_size, dst);
        return true;
    }

    auto num_frames = src_size / SrcFormat::sample_size / num_channels;

    if (num_frames != dst_size / DstFormat::sample_size / num_channels) {
        return false;  // Unequal amount of frames between src and dst
    }

    if constexpr (std::is_same_v<SrcInterleaving, interleaving::interleaved>) {
        if constexpr (std::is_same_v<DstInterleaving, interleaving::interleaved>) {
            // Interleaved src, interleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto src_i = i * SrcFormat::sample_size;
                const auto dst_i = i * DstFormat::sample_size;
                convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        } else {
            // Interleaved src, noninterleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i % num_channels;
                const auto src_i = i * SrcFormat::sample_size;
                const auto dst_i = (ch * num_frames + i / num_channels) * DstFormat::sample_size;
                convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        }
    } else {
        if constexpr (std::is_same_v<DstInterleaving, interleaving::interleaved>) {
            // Noninterleaved src, interleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i % num_frames;
                const auto src_i = (ch * num_frames + i / num_channels) * SrcFormat::sample_size;
                const auto dst_i = i * DstFormat::sample_size;
                convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        } else {
            // Noninterleaved src, noninterleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i % num_frames;
                const auto src_i = (ch * num_frames + i / num_channels) * SrcFormat::sample_size;
                const auto dst_i = (ch * num_frames + i / num_channels) * DstFormat::sample_size;
                convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        }
    }

    return true;
}

}  // namespace rav::audio_data
