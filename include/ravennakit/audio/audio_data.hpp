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
        static constexpr bool is_little_endian = true;

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
        static constexpr bool is_little_endian = false;

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
#if RAV_LITTLE_ENDIAN
        static constexpr bool is_little_endian = true;
#else
        static constexpr bool is_little_endian = false;
#endif

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

    struct uint8 {
        static constexpr size_t sample_size = 1;
        using type = uint8_t;
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

namespace detail {
    template<class Format, class ByteOrder>
    typename Format::type read_sample(const uint8_t* data) {
        static_assert(
            Format::sample_size <= sizeof(typename Format::type), "sample_size is larger than the size of the value"
        );
        typename Format::type value;
        std::memcpy(std::addressof(value), data, Format::sample_size);
        const auto swapped = ByteOrder::swap(value);
        const auto shifted = swapped >> (sizeof(value) - Format::sample_size) * 8;
        return static_cast<typename Format::type>(shifted);
    }

    template<class Format, class ByteOrder>
    void write_sample(uint8_t* data, typename Format::type value) {
        static_assert(
            Format::sample_size <= sizeof(typename Format::type), "sample_size is larger than the size of the value"
        );
        value = ByteOrder::swap(value);
        if constexpr (ByteOrder::is_little_endian) {
            std::memcpy(data, std::addressof(value), Format::sample_size);
        } else {
            std::memcpy(
                data, reinterpret_cast<uint8_t*>(std::addressof(value)) + (sizeof(value) - Format::sample_size),
                Format::sample_size
            );
        }
    }

    template<class SrcFormat, class SrcByteOrder, class DstFormat, class DstByteOrder>
    static void convert_sample(const uint8_t* src, uint8_t* dst) {
        if constexpr (std::is_same_v<SrcFormat, DstFormat> && std::is_same_v<SrcByteOrder, DstByteOrder>) {
            std::memcpy(dst, src, SrcFormat::sample_size);
        } else if constexpr (std::is_same_v<SrcFormat, DstFormat>) {
            // Only byte order differs
            write_sample<DstFormat, DstByteOrder>(
                dst, static_cast<typename DstFormat::type>(read_sample<SrcFormat, SrcByteOrder>(src))
            );
        } else {
            const auto src_sample = read_sample<SrcFormat, SrcByteOrder>(src);

            if constexpr (std::is_same_v<SrcFormat, format::uint8>) {
                if constexpr (std::is_same_v<DstFormat, format::int8>) {
                    write_sample<DstFormat, DstByteOrder>(dst, static_cast<int8_t>(src_sample - 0x80));
                    return;
                }
            } else if constexpr (std::is_same_v<SrcFormat, format::int8>) {
                if constexpr (std::is_same_v<DstFormat, format::int16>) {
                    write_sample<DstFormat, DstByteOrder>(
                        dst, static_cast<int16_t>(static_cast<uint8_t>(src_sample) << 8)
                    );
                    return;
                }
            } else if constexpr (std::is_same_v<SrcFormat, format::int16>) {
                if constexpr (std::is_same_v<DstFormat, format::int24>) {
                    write_sample<DstFormat, DstByteOrder>(
                        dst, static_cast<int32_t>(static_cast<uint32_t>(src_sample) << 8)
                    );
                    return;
                } else if constexpr (std::is_same_v<DstFormat, format::int24in32>) {
                    write_sample<DstFormat, DstByteOrder>(
                        dst, static_cast<int32_t>(static_cast<uint32_t>(src_sample) << 16)
                    );
                    return;
                } else if constexpr (std::is_same_v<DstFormat, format::int32>) {
                    write_sample<DstFormat, DstByteOrder>(
                        dst, static_cast<int32_t>(static_cast<uint32_t>(src_sample) << 16)
                    );
                    return;
                }
            } else if constexpr (std::is_same_v<SrcFormat, format::int24>) {
                if constexpr (std::is_same_v<DstFormat, format::f32>) {
                    // auto f = (static_cast<float>(src_sample) + 8388608.0f) * 0.00000011920929665621f - 1.f;
                    auto f = static_cast<float>(src_sample) * 0.00000011920928955078125f;
                    write_sample<DstFormat, DstByteOrder>(dst, f);
                    return;
                } else if constexpr (std::is_same_v<DstFormat, format::f64>) {
                    // auto f = (static_cast<double>(src_sample) + 8388608.0) * 0.00000011920929665621 - 1.0;
                    auto f = static_cast<double>(src_sample) * 0.00000011920928955078125;
                    write_sample<DstFormat, DstByteOrder>(dst, f);
                    return;
                }
            }

            RAV_ASSERT_FALSE("Conversion not implemented");
        }
    }
}  // namespace detail

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
    } else if (std::is_same_v<SrcFormat, DstFormat> && std::is_same_v<SrcInterleaving, DstInterleaving>) {
        RAV_ASSERT(src_size == dst_size);

        std::memcpy(dst, src, src_size);

        if constexpr (SrcByteOrder::is_little_endian == DstByteOrder::is_little_endian) {
            return true;  // No need for swapping
        }

        for (size_t i = 0; i < dst_size; i += SrcFormat::sample_size) {
            rav::byte_order::swap_bytes(dst + i, SrcFormat::sample_size);
        }

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
                detail::convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        } else {
            // Interleaved src, noninterleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i % num_channels;
                const auto src_i = i * SrcFormat::sample_size;
                const auto dst_i = (ch * num_frames + i / num_channels) * DstFormat::sample_size;
                detail::convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        }
    } else {
        if constexpr (std::is_same_v<DstInterleaving, interleaving::interleaved>) {
            // Noninterleaved src, interleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i % num_frames;
                const auto src_i = (ch * num_frames + i / num_channels) * SrcFormat::sample_size;
                const auto dst_i = i * DstFormat::sample_size;
                detail::convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        } else {
            // Noninterleaved src, noninterleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i % num_frames;
                const auto src_i = (ch * num_frames + i / num_channels) * SrcFormat::sample_size;
                const auto dst_i = (ch * num_frames + i / num_channels) * DstFormat::sample_size;
                detail::convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        }
    }

    return true;
}

}  // namespace rav::audio_data
