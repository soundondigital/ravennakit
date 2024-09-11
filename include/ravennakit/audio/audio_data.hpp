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
    struct interleaved {
        static size_t get_sample_index(
            const size_t channel_index, const size_t frame_index, const size_t num_channels,
            [[maybe_unused]] const size_t num_frames
        ) {
            return frame_index * num_channels + channel_index;
        }
    };

    struct noninterleaved {
        static size_t get_sample_index(
            const size_t channel_index, const size_t frame_index, [[maybe_unused]] const size_t num_channels,
            const size_t num_frames
        ) {
            return channel_index * num_frames + frame_index;
        }
    };
}  // namespace interleaving

namespace byte_order {
    struct le {
        template<class T>
        static T read(const uint8_t* data) {
            return rav::byte_order::read_le<T>(data);
        }

        template<class T>
        static void write(uint8_t* data, const T value) {
            return rav::byte_order::write_le<T>(data, value);
        }
    };

    struct be {
        template<class T>
        static T read(const uint8_t* data) {
            return rav::byte_order::read_be<T>(data);
        }

        template<class T>
        static void write(uint8_t* data, T value) {
            return rav::byte_order::write_be<T>(data, value);
        }
    };

    struct ne {
        template<class T>
        static T read(const uint8_t* data) {
            return rav::byte_order::read_ne<T>(data);
        }

        template<class T>
        static void write(uint8_t* data, T value) {
            return rav::byte_order::write_ne<T>(data, value);
        }
    };
}  // namespace byte_order

namespace format {
    template<class ByteOrder>
    struct int8 {
        static constexpr size_t sample_size = 1;

        static int8_t read(const uint8_t* data) {
            return ByteOrder::template read<int8_t>(data);
        }
    };

    template<class ByteOrder>
    struct int16 {
        static constexpr size_t sample_size = 2;

        static int16_t read(const uint8_t* data) {
            return ByteOrder::template read<int16_t>(data);
        }

        static void write(uint8_t* data, const int8_t value) {
            ByteOrder::template write<int16_t>(data, value);
        }
    };

    template<class ByteOrder>
    struct int24 {
        static constexpr size_t sample_size = 3;
    };

    template<class ByteOrder>
    struct int24in32 {
        static constexpr size_t sample_size = 4;
    };

    template<class ByteOrder>
    struct int32 {
        static constexpr size_t sample_size = 4;

        static int32_t read(const uint8_t* data) {
            return ByteOrder::template read<int32_t>(data);
        }
    };

    template<class ByteOrder>
    struct int64 {
        static constexpr size_t sample_size = 8;

        static int64_t read(const uint8_t* data) {
            return ByteOrder::template read<int64_t>(data);
        }
    };

    template<class ByteOrder>
    struct uint8 {
        static constexpr size_t sample_size = 1;

        static uint8_t read(const uint8_t* data) {
            return ByteOrder::template read<uint8_t>(data);
        }
    };

    template<class ByteOrder>
    struct uint16 {
        static constexpr size_t sample_size = 2;

        static uint16 read(const uint8_t* data) {
            return ByteOrder::template read<uint16>(data);
        }
    };

    template<class ByteOrder>
    struct uint24 {
        static constexpr size_t sample_size = 3;
    };

    template<class ByteOrder>
    struct uint24in32 {
        static constexpr size_t sample_size = 4;
    };

    template<class ByteOrder>
    struct uint32 {
        static constexpr size_t sample_size = 4;

        static uint32 read(const uint8_t* data) {
            return ByteOrder::template read<uint32>(data);
        }
    };

    template<class ByteOrder>
    struct uint64 {
        static constexpr size_t sample_size = 8;

        static uint64 read(const uint8_t* data) {
            return ByteOrder::template read<uint64>(data);
        }
    };

    template<class ByteOrder>
    struct f32 {
        static constexpr size_t sample_size = 4;

        static float read(const uint8_t* data) {
            return ByteOrder::template read<float>(data);
        }
    };

    template<class ByteOrder>
    struct f64 {
        static constexpr size_t sample_size = 8;

        static double read(const uint8_t* data) {
            return ByteOrder::template read<double>(data);
        }
    };
}  // namespace format

template<class SrcFormat, class DstFormat>
static void convert_sample(const uint8_t* src, uint8_t* dst) {
    if constexpr (std::is_same_v<SrcFormat, DstFormat>) {
        std::memcpy(dst, src, SrcFormat::sample_size);
    } else {
        DstFormat::write(dst, SrcFormat::read(src));
    }
}

template<class SrcFormat, class SrcInterleaving, class DstFormat, class DstInterleaving>
static bool
convert(const uint8_t* src, const size_t src_size, uint8_t* dst, const size_t dst_size, const size_t num_channels) {
    if (src_size % SrcFormat::sample_size != 0) {
        return false;  // src_size is not a multiple of sample size
    }

    if (dst_size % DstFormat::sample_size != 0) {
        return false;  // dst_size is not a multiple of sample size
    }

    // Shortcut for when no conversion has to be done
    if constexpr (std::is_same_v<SrcFormat, DstFormat> && std::is_same_v<SrcInterleaving, DstInterleaving>) {
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
                convert_sample<SrcFormat, DstFormat>(src + src_i, dst + dst_i);
            }
            return true;
        } else {
            // Interleaved src, noninterleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i / SrcFormat::sample_size % num_channels;
                const auto src_i = i * SrcFormat::sample_size;
                const auto dst_i = ch * num_frames + i / num_channels;
                convert_sample<SrcFormat, DstFormat>(src + src_i, dst + dst_i);
            }
            return true;
        }
    } else {
        if constexpr (std::is_same_v<DstInterleaving, interleaving::interleaved>) {
            // Noninterleaved src, interleaved dst
            RAV_ASSERT_FALSE("Not implemented");
        } else {
            // Noninterleaved src, noninterleaved dst
            RAV_ASSERT_FALSE("Not implemented");
        }
    }

    return false;
}

}  // namespace rav::audio_data
