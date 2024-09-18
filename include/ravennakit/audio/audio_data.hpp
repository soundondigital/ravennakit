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

        static uint64_t read(const uint8_t* data, const size_t size) {
            uint64_t value {};
            RAV_ASSERT(size <= sizeof(value));
            std::memcpy(std::addressof(value), data, size);
            if constexpr (big_endian) {
                value = rav::byte_order::swap_bytes(value);
            }
            return value;
        }

        template<class T>
        static void write(uint8_t* data, const size_t size, T value) {
            RAV_ASSERT(size <= sizeof(value));
            if constexpr (big_endian) {
                value = rav::byte_order::swap_bytes(value);
            }
            std::memcpy(data, std::addressof(value), size);
        }
    };

    struct be {
        static constexpr bool is_little_endian = false;

        static uint64_t read(const uint8_t* data, const size_t size) {
            uint64_t value {};
            RAV_ASSERT(size <= sizeof(value));
            if constexpr (little_endian) {
                std::memcpy(reinterpret_cast<uint8_t*>(std::addressof(value)) + (sizeof(value) - size), data, size);
                value = rav::byte_order::swap_bytes(value);
            } else {
                std::memcpy(std::addressof(value), data, size);
            }
            return value;
        }

        template<class T>
        static void write(uint8_t* data, const size_t size, T value) {
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

        static uint64_t read(const uint8_t* data, const size_t size) {
            uint64_t value {};
            RAV_ASSERT(size <= sizeof(value));
            if constexpr (little_endian) {
                std::memcpy(std::addressof(value), data, size);
            } else {
                RAV_ASSERT_FALSE("Implement me");
            }
            return value;
        }

        template<class T>
        static void write(uint8_t* data, const size_t size, T value) {
            RAV_ASSERT(size <= sizeof(value));
            if constexpr (little_endian) {
                std::memcpy(data, std::addressof(value), size);
            } else {
                RAV_ASSERT_FALSE("Implement me");
            }
        }
    };
}  // namespace byte_order

namespace format {
    struct int8 {
        static constexpr size_t size = 1;
    };

    struct int16 {
        static constexpr size_t size = 2;
    };

    struct int24 {
        static constexpr size_t size = 3;
    };

    struct int24in32 {
        static constexpr size_t size = 4;
    };

    struct int32 {
        static constexpr size_t size = 4;
    };

    struct uint8 {
        static constexpr size_t size = 1;
    };

    struct f32 {
        static constexpr size_t size = 4;
    };

    struct f64 {
        static constexpr size_t size = 8;
    };
}  // namespace format

namespace detail {
    class int24_t {
      public:
        int24_t() = default;
        ~int24_t() = default;

        explicit int24_t(const float value) : int24_t(static_cast<int32_t>(value)) {}

        explicit int24_t(const double value) : int24_t(static_cast<int32_t>(value)) {}

        explicit int24_t(const int32_t value) {
            std::memcpy(data, std::addressof(value), 3);
        }

        int24_t(const int24_t& other) = default;
        int24_t(int24_t&& other) noexcept = default;
        int24_t& operator=(const int24_t& other) = default;
        int24_t& operator=(int24_t&& other) noexcept = default;

        explicit operator int32_t() const {
            int32_t value {};
            std::memcpy(std::addressof(value), data, 3);

            // If the value is negative, sign-extend it
            if (value << 8 < 0) {
                return value * -1;
            }

            return value;
        }

      private:
        uint8_t data[3] {};
    };

    static_assert(sizeof(int24_t) == 3);

    template<class SrcFormat, class SrcByteOrder, class DstFormat, class DstByteOrder>
    static void convert_sample(const uint8_t* src, uint8_t* dst) {
        if constexpr (std::is_same_v<SrcFormat, DstFormat> && std::is_same_v<SrcByteOrder, DstByteOrder>) {
            std::memcpy(dst, src, SrcFormat::size);
        } else if constexpr (std::is_same_v<SrcFormat, DstFormat>) {
            // Only byte order differs
            DstByteOrder::write(dst, DstFormat::size, SrcByteOrder::read(src, SrcFormat::size));
        } else {
            const auto src_sample = SrcByteOrder::read(src, SrcFormat::size);

            if constexpr (std::is_same_v<SrcFormat, format::uint8>) {
                if constexpr (std::is_same_v<DstFormat, format::int8>) {
                    DstByteOrder::write(dst, DstFormat::size, src_sample - 0x80);
                } else {
                    RAV_ASSERT_FALSE("Conversion not available");
                }
            } else if constexpr (std::is_same_v<SrcFormat, format::int8>) {
                if constexpr (std::is_same_v<DstFormat, format::int16>) {
                    DstByteOrder::write(dst, DstFormat::size, src_sample << 8);
                } else {
                    RAV_ASSERT_FALSE("Conversion not available");
                }
            } else if constexpr (std::is_same_v<SrcFormat, format::int16>) {
                if constexpr (std::is_same_v<DstFormat, format::int24>) {
                    DstByteOrder::write(dst, DstFormat::size, src_sample << 8);
                } else if constexpr (std::is_same_v<DstFormat, format::int24in32>) {
                    DstByteOrder::write(dst, DstFormat::size, src_sample << 16);
                } else if constexpr (std::is_same_v<DstFormat, format::int32>) {
                    DstByteOrder::write(dst, DstFormat::size, src_sample << 16);
                } else if constexpr (std::is_same_v<DstFormat, format::f32>) {
                    const bool is_negative = static_cast<int64_t>(src_sample) << 48 < 0;
                    auto f = static_cast<float>(src_sample) * 0.000030517578125f * (is_negative ? -1 : 1);
                    DstByteOrder::write(dst, DstFormat::size, f);
                } else if constexpr (std::is_same_v<DstFormat, format::f64>) {
                    const bool is_negative = static_cast<int64_t>(src_sample) << 48 < 0;
                    auto f = static_cast<double>(src_sample) * 0.000030517578125f * (is_negative ? -1 : 1);
                    DstByteOrder::write(dst, DstFormat::size, f);
                } else {
                    RAV_ASSERT_FALSE("Conversion not available");
                }
            } else if constexpr (std::is_same_v<SrcFormat, format::int24>) {
                if constexpr (std::is_same_v<DstFormat, format::f32>) {
                    const bool is_negative = static_cast<int64_t>(src_sample) << 40 < 0;
                    auto f = static_cast<float>(src_sample) * 0.00000011920928955078125f * (is_negative ? -1 : 1);
                    DstByteOrder::write(dst, DstFormat::size, f);
                } else if constexpr (std::is_same_v<DstFormat, format::f64>) {
                    const bool is_negative = static_cast<int64_t>(src_sample) << 40 < 0;
                    auto f = static_cast<double>(src_sample) * 0.00000011920928955078125f * (is_negative ? -1 : 1);
                    DstByteOrder::write(dst, DstFormat::size, f);
                } else {
                    RAV_ASSERT_FALSE("Conversion not available");
                }
            } else if constexpr (std::is_same_v<SrcFormat, format::f32>) {
                if constexpr (std::is_same_v<DstFormat, format::int16>) {
                    const auto scaled = *reinterpret_cast<const float*>(&src_sample) * 32767.0f;
                    DstByteOrder::write(dst, DstFormat::size, static_cast<int16_t>(scaled));
                } else if constexpr (std::is_same_v<DstFormat, format::int24>) {
                    const auto scaled = *reinterpret_cast<const float*>(&src_sample) * 8388607.f;
                    DstByteOrder::write(dst, DstFormat::size, static_cast<int24_t>(scaled));
                } else {
                    RAV_ASSERT_FALSE("Conversion not available");
                }
            } else if constexpr (std::is_same_v<SrcFormat, format::f64>) {
                RAV_ASSERT_FALSE("Conversion not available");
            } else {
                RAV_ASSERT_FALSE("Conversion not available");
            }
        }
    }
}  // namespace detail

template<
    class SrcFormat, class SrcByteOrder, class SrcInterleaving, class DstFormat, class DstByteOrder,
    class DstInterleaving>
static bool
convert(const uint8_t* src, const size_t src_size, uint8_t* dst, const size_t dst_size, const size_t num_channels) {
    if (src_size % SrcFormat::size != 0) {
        return false;  // src_size is not a multiple of sample size
    }

    if (dst_size % DstFormat::size != 0) {
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

        for (size_t i = 0; i < dst_size; i += SrcFormat::size) {
            rav::byte_order::swap_bytes(dst + i, SrcFormat::size);
        }

        return true;
    }

    auto num_frames = src_size / SrcFormat::size / num_channels;

    if (num_frames != dst_size / DstFormat::size / num_channels) {
        return false;  // Unequal amount of frames between src and dst
    }

    if constexpr (std::is_same_v<SrcInterleaving, interleaving::interleaved>) {
        if constexpr (std::is_same_v<DstInterleaving, interleaving::interleaved>) {
            // Interleaved src, interleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto src_i = i * SrcFormat::size;
                const auto dst_i = i * DstFormat::size;
                detail::convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        } else {
            // Interleaved src, noninterleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i % num_channels;
                const auto src_i = i * SrcFormat::size;
                const auto dst_i = (ch * num_frames + i / num_channels) * DstFormat::size;
                detail::convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        }
    } else {
        if constexpr (std::is_same_v<DstInterleaving, interleaving::interleaved>) {
            // Noninterleaved src, interleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i % num_frames;
                const auto src_i = (ch * num_frames + i / num_channels) * SrcFormat::size;
                const auto dst_i = i * DstFormat::size;
                detail::convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        } else {
            // Noninterleaved src, noninterleaved dst
            for (size_t i = 0; i < num_frames * num_channels; ++i) {
                const auto ch = i % num_frames;
                const auto src_i = (ch * num_frames + i / num_channels) * SrcFormat::size;
                const auto dst_i = (ch * num_frames + i / num_channels) * DstFormat::size;
                detail::convert_sample<SrcFormat, SrcByteOrder, DstFormat, DstByteOrder>(src + src_i, dst + dst_i);
            }
        }
    }

    return true;
}

}  // namespace rav::audio_data
