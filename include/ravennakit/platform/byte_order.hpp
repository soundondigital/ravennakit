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
#include <memory>

// This defines RAV_LITTLE_ENDIAN and RAV_BIG_ENDIAN macros indicating the byte order of the system.
// TODO: Test on a big endian system
#if (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)) || (defined(__BIG_ENDIAN__) && __BIG_ENDIAN__)
    #define RAV_LITTLE_ENDIAN 0
    #define RAV_BIG_ENDIAN 1
    #warning "Big endian systems have not been tested yet"
#else
    #define RAV_LITTLE_ENDIAN 1
    #define RAV_BIG_ENDIAN 0
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

namespace rav::byte_order {

/**
 * @tparam Type The type of the value to swap.
 * @param value The value to swap.
 * @return The value with the bytes swapped.
 */
template<typename Type>
Type swap_bytes(Type value) {
    if constexpr (sizeof(Type) == 2) {
        value = RAV_BYTE_SWAP_16(value);
    } else if constexpr (sizeof(Type) == 4) {
        value = RAV_BYTE_SWAP_32(value);
    } else if constexpr (sizeof(Type) == 8) {
        value = RAV_BYTE_SWAP_64(value);
    }
    return value;
}

/**
 * Specialization of swap_bytes for floats.
 * @param value The value to swap.
 * @return The value with the bytes swapped.
 */
template<>
inline float swap_bytes<float>(const float value) {
    static_assert(sizeof(float) == sizeof(uint32_t), "Float must be 32 bits");
    union { float f; uint32_t i; } u = { value };
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
    union { double f; uint64_t i; } u = { value };
    u.i = RAV_BYTE_SWAP_64(u.i);
    return u.f;
}

/**
 * Reads a value from the given data in native byte order (not to be confused with network-endian).
 * @tparam Type The type of the value to read.
 * @param data The data which holds the encoded value.
 * @return The decoded value.
 */
template<typename Type>
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
template<typename Type>
Type read_be(const uint8_t* data) {
    if constexpr (RAV_BIG_ENDIAN) {
        return read_ne<Type>(data);
    } else {
        return swap_bytes(read_ne<Type>(data));
    }
}

/**
 * Reads a little-endian value from the given data.
 * @tparam Type The type of the value to read.
 * @param data The data which holds the encoded value.
 * @return The decoded value.
 */
template<typename Type>
Type read_le(const uint8_t* data) {
    if constexpr (RAV_LITTLE_ENDIAN) {
        return read_ne<Type>(data);
    } else {
        return swap_bytes(read_ne<Type>(data));
    }
}

/**
 * Writes a value to the given destination in native byte order (not to be confused with network-endian).
 * @tparam Type The type of the value to write.
 * @param dst The destination where the value should be written.
 * @param value The value to write.
 */
template<typename Type>
void write_ne(uint8_t* dst, Type value) {
    std::memcpy(dst, std::addressof(value), sizeof(Type));
}

/**
 * Writes a big-endian value to the given destination.
 * @tparam Type The type of the value to write.
 * @param dst The destination where the value should be written.
 * @param value The value to write.
 */
template<typename Type>
void write_be(uint8_t* dst, Type value) {
    if constexpr (RAV_BIG_ENDIAN) {
        write_ne(dst, value);
    } else {
        write_ne(dst, swap_bytes(value));
    }
}

/**
 * Writes a little-endian value to the given destination.
 * @tparam Type The type of the value to write.
 * @param dst The destination where the value should be written.
 * @param value The value to write.
 */
template<typename Type>
void write_le(uint8_t* dst, Type value) {
    if constexpr (RAV_LITTLE_ENDIAN) {
        write_ne(dst, value);
    } else {
        write_ne(dst, swap_bytes(value));
    }
}

}  // namespace rav::byte_order
