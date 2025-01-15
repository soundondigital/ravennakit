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
#include <algorithm>
#include <cstring>

namespace rav {

/**
 * Custom 48 bit unsigned integer type. Primarily used for PTP, but can be used for other purposes.
 */
class uint48_t {
  public:
    uint48_t() = default;

    /**
     * Construct an uint48_t from a uint64_t value. The value is truncated to 48 bits.
     * @param value The value to store in the uint48_t.
     */
    // ReSharper disable once CppNonExplicitConvertingConstructor
    uint48_t(uint64_t value) {  // NOLINT(google-explicit-constructor)
        std::memcpy(data_, std::addressof(value), sizeof(data_));
    }

    /**
     * Get a uint64_t representation of the int48_t.
     */
    explicit operator uint64_t() const {
        uint64_t value {};
        std::memcpy(std::addressof(value), data_, sizeof(data_));
        return value;
    }

    /**
     * @returns A pointer to the data stored in the int48_t.
     */
    [[nodiscard]] const uint8_t* data() const {
        return data_;
    }

    /**
     * @returns The value stored in the int48_t as an int64_t.
     */
    [[nodiscard]] uint64_t to_uint64() const {
        return static_cast<uint64_t>(*this);
    }

    [[nodiscard]] bool operator==(const uint48_t& other) const {
        return static_cast<uint64_t>(*this) == static_cast<uint64_t>(other);
    }

    [[nodiscard]] bool operator!=(const uint48_t& other) const {
        return !(*this == other);
    }

    [[nodiscard]] bool operator<(const uint48_t& other) const {
        return static_cast<uint64_t>(*this) < static_cast<uint64_t>(other);
    }

    [[nodiscard]] bool operator>(const uint48_t& other) const {
        return static_cast<uint64_t>(*this) > static_cast<uint64_t>(other);
    }

    [[nodiscard]] bool operator<=(const uint48_t& other) const {
        return static_cast<uint64_t>(*this) <= static_cast<uint64_t>(other);
    }

    [[nodiscard]] bool operator>=(const uint48_t& other) const {
        return static_cast<uint64_t>(*this) >= static_cast<uint64_t>(other);
    }

  private:
    static constexpr uint64_t k_max = 0xffffffffffff;
    static constexpr uint64_t k_min = 0x0;

    uint8_t data_[6] {};
};

// Ensure that int24_t is 6 bytes in size
static_assert(sizeof(uint48_t) == 6);

}  // namespace rav
