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
#include <algorithm>

namespace rav {

/**
 * A custom type to represent a 3-byte audio sample. The size of this class is always 3 bytes to make it suitable to
 * memcpy to/from audio buffers.
 */
class int24_t {
  public:
    int24_t() = default;
    ~int24_t() = default;

    /**
     * Construct an int24_t from a float value.
     * @param value The value to store in the int24_t.
     */
    explicit int24_t(const float value) : int24_t(static_cast<int32_t>(value)) {}

    /**
     * Construct an int24_t from a double value.
     * @param value The value to store in the int24_t.
     */
    explicit int24_t(const double value) : int24_t(static_cast<int32_t>(value)) {}

    /**
     * Construct an int24_t from an int32_t value. The value is truncated to 24 bits.
     * @param value The value to store in the int24_t.
     */
    // ReSharper disable once CppNonExplicitConvertingConstructor
    int24_t(int32_t value) {  // NOLINT(google-explicit-constructor)
        value = std::clamp(value, k_min, k_max);
        std::memcpy(data_, std::addressof(value), sizeof(data_));
    }

    int24_t(const int24_t& other) = default;
    int24_t(int24_t&& other) noexcept = default;
    int24_t& operator=(const int24_t& other) = default;
    int24_t& operator=(int24_t&& other) noexcept = default;

    bool operator==(const int32_t other) const {
        return static_cast<int32_t>(*this) == other;
    }

    /**
     * @returns The value stored in the int24_t as an int32_t.
     */
    explicit operator int32_t() const {
        int32_t value {};
        std::memcpy(std::addressof(value), data_, sizeof(data_));
        return value << 8 >> 8; // Sign extend the 24-bit value to 32 bits
    }

  private:
    static constexpr int32_t k_max = 0x7fffff;
    static constexpr int32_t k_min = -0x800000;

    uint8_t data_[3] {};
};

// Ensure that int24_t is 3 bytes in size
static_assert(sizeof(int24_t) == 3);

}  // namespace rav
