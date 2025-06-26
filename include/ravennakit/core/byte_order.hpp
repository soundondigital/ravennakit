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

#include <cstdint>
#include <cstring>

#include "ravennakit/core/exception.hpp"

// This defines RAV_LITTLE_ENDIAN and RAV_BIG_ENDIAN macros indicating the byte order of the system.
#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || (defined(__BIG_ENDIAN__) && __BIG_ENDIAN__)
    #define RAV_LITTLE_ENDIAN 0
    #define RAV_BIG_ENDIAN 1

namespace rav {
static constexpr bool little_endian = false;
static constexpr bool big_endian = true;
}  // namespace rav

    #warning "Big endian systems have not been tested yet"
#else
    #define RAV_LITTLE_ENDIAN 1
    #define RAV_BIG_ENDIAN 0

namespace rav {
static constexpr bool little_endian = true;
static constexpr bool big_endian = false;
}  // namespace rav
#endif

// This defines the byte swap functions for 16, 32, and 64 bit integers, using platform-specific implementations if
// possible.
#if defined(__clang__) || defined(__GNUC__)
    #define RAV_BYTE_SWAP_16(x) __builtin_bswap16(x)
    #define RAV_BYTE_SWAP_32(x) __builtin_bswap32(x)
    #define RAV_BYTE_SWAP_64(x) __builtin_bswap64(x)
#elif defined(_MSC_VER)
    #include <intrin.h>
    #define RAV_BYTE_SWAP_16(x) _byteswap_ushort(x)
    #define RAV_BYTE_SWAP_32(x) _byteswap_ulong(x)
    #define RAV_BYTE_SWAP_64(x) _byteswap_uint64(x)
#else
    #error "Unsupported compiler"
#endif

namespace rav {

/**
 * Swaps given amount of bytes in the given data, in place.
 * @param data The data to swap.
 * @param size The size of the data (in bytes).
 */
inline void swap_bytes(uint8_t* data, const size_t size) {
    // Check for null data pointer or zero size
    if (data == nullptr || size == 0) {
        return;
    }

    for (size_t i = 0; i < size / 2; ++i) {
        std::swap(data[i], data[size - i - 1]);
    }
}

/**
 * Swaps given amount of bytes in the given data, in place, with the given stride.
 * @param data The data to swap.
 * @param size The size of the data (in bytes).
 * @param stride The stride of the data (in bytes).
 */
inline void swap_bytes(uint8_t* data, const size_t size, const size_t stride) {
    // Check for null data pointer or zero size
    if (data == nullptr || size == 0 || stride <= 1) {
        return;
    }

    for (size_t i = 0; i < size; i += stride) {
        for (size_t j = 0; j < stride / 2; ++j) {
            std::swap(data[i + j], data[i + stride - j - 1]);
        }
    }
}

/**
 * @tparam Type The type of the value to swap.
 * @param value The value to swap.
 * @return The value with the bytes swapped.
 */
template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
Type swap_bytes(Type value) {
    // Note: this function hasn't been benchmarked.
    if constexpr (sizeof(Type) == 1) {
        return value;
    } else if constexpr (sizeof(Type) == 2) {
        return static_cast<Type>(RAV_BYTE_SWAP_16(static_cast<uint16_t>(value)));
    } else if constexpr (sizeof(Type) == 4) {
        return static_cast<Type>(RAV_BYTE_SWAP_32(static_cast<uint32_t>(value)));
    } else if constexpr (sizeof(Type) == 8) {
        return static_cast<Type>(RAV_BYTE_SWAP_64(static_cast<uint64_t>(value)));
    } else {
        swap_bytes(reinterpret_cast<uint8_t*>(&value), sizeof(value));
        return value;
    }
}

/**
 * Specialization of swap_bytes for floats.
 * @param value The value to swap.
 * @return The value with the bytes swapped.
 */
template<>
inline float swap_bytes<float>(const float value) {
    static_assert(sizeof(float) == sizeof(uint32_t), "Float must be 32 bits");

    union {
        float f;
        uint32_t i;
    } u = {value};

    u.i = RAV_BYTE_SWAP_32(u.i);
    return u.f;
}

/**
 * Specialization of swap_bytes for doubles.
 * @param value The value to swap.
 * @return The value with the bytes swapped.
 */
template<>
inline double swap_bytes<double>(const double value) {
    static_assert(sizeof(double) == sizeof(uint64_t), "Double must be 64 bits");

    union {
        double f;
        uint64_t i;
    } u = {value};

    u.i = RAV_BYTE_SWAP_64(u.i);
    return u.f;
}

/**
 * Swaps the bytes of the given value if the system is little-endian.
 * @tparam Type The type of the value to swap.
 * @param value The value to swap.
 * @return The value with the bytes swapped.
 */
template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
Type swap_if_le(const Type value) {
    if constexpr (little_endian) {
        return swap_bytes(value);
    } else {
        return value;
    }
}

/**
 * Swaps the bytes of the given value if the system is big-endian.
 * @tparam Type The type of the value to swap.
 * @param value The value to swap.
 * @return The value with the bytes swapped.
 */
template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
Type swap_if_be(const Type value) {
    if constexpr (big_endian) {
        return swap_bytes(value);
    } else {
        return value;
    }
}

/**
 * Reads a value from the given data in native byte order (not to be confused with network-endian).
 * @tparam Type The type of the value to read.
 * @param data The data which holds the encoded value.
 * @return The decoded value.
 */
template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
Type read_ne(const uint8_t* data) {
    Type value;
    std::memcpy(std::addressof(value), data, sizeof(Type));
    return value;
}

/**
 * Reads a big-endian value from the given data.
 * @tparam Type The type of the value to read.
 * @param data The data which holds the encoded value.
 * @return The decoded value.
 */
template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
Type read_be(const uint8_t* data) {
    return swap_if_le(read_ne<Type>(data));
}

/**
 * Reads a little-endian value from the given data.
 * @tparam Type The type of the value to read.
 * @param data The data which holds the encoded value.
 * @return The decoded value.
 */
template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
Type read_le(const uint8_t* data) {
    return swap_if_be(read_ne<Type>(data));
}

/**
 * Writes a value to the given destination in native byte order (not to be confused with network-endian).
 * @tparam Type The type of the value to write.
 * @param dst The destination where the value should be written.
 * @param value The value to write.
 */
template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
void write_ne(uint8_t* dst, const Type value) {
    std::memcpy(dst, std::addressof(value), sizeof(Type));
}

/**
 * Writes a big-endian value to the given destination.
 * @tparam Type The type of the value to write.
 * @param dst The destination where the value should be written.
 * @param value The value to write.
 */
template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
void write_be(uint8_t* dst, const Type value) {
    write_ne(dst, swap_if_le(value));
}

/**
 * Writes a little-endian value to the given destination.
 * @tparam Type The type of the value to write.
 * @param dst The destination where the value should be written.
 * @param value The value to write.
 */
template<typename Type, std::enable_if_t<std::is_trivially_copyable_v<Type>, bool> = true>
void write_le(uint8_t* dst, const Type value) {
    write_ne(dst, swap_if_be(value));
}

/**
 * Checks if the system is little-endian at runtime.
 * @return True if the system is little-endian, false otherwise.
 */
inline bool is_little_endian_at_runtime() noexcept {
    static size_t native_one = 1;
    return reinterpret_cast<uint8_t*>(&native_one)[0] == 1;
}

/**
 * Validates whether the compiled byte order matches the runtime byte order. If not throws an exception.
 */
inline void validate_byte_order() {
#if RAV_LITTLE_ENDIAN
    if (!is_little_endian_at_runtime()) {
        RAV_THROW_EXCEPTION("Compile-time and runtime byte order do not match");
    }
#else
    if (is_little_endian_runtime()) {
        RAV_THROW_EXCEPTION("Compile-time and runtime byte order do not match");
    }
#endif
}

}  // namespace rav
