/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once

#include <optional>
#include <limits>

namespace rav::safe_math {

/**
 * Adds a and b, returning std::nullopt if the operation would overflow.
 * @tparam T The type of the values to add.
 * @param a The first value.
 * @param b The second value.
 * @return The sum of a and b, or std::nullopt if the operation would overflow.
 */
template<typename T>
[[nodiscard]] std::optional<T> add(T a, T b) {
    static_assert(std::is_integral_v<T>, "T must be an integral type");

    if ((b > 0 && a > std::numeric_limits<T>::max() - b) || (b < 0 && a < std::numeric_limits<T>::min() - b)) {
        return std::nullopt;
    }

    return a + b;
}

/**
 * Subtracts b from a, returning std::nullopt if the operation would overflow.
 * @tparam T The type of the values to subtract.
 * @param a The first value.
 * @param b The second value.
 * @return The difference of a and b, or std::nullopt if the operation would overflow.
 */
template<typename T>
[[nodiscard]] std::optional<T> sub(T a, T b) {
    static_assert(std::is_integral_v<T>, "T must be an integral type");

    // Check for underflow and overflow
    if ((b > 0 && a < std::numeric_limits<T>::min() + b) || (b < 0 && a > std::numeric_limits<T>::max() + b)) {
        return std::nullopt;
    }

    return a - b;
}

/**
 * Multiplies a and b, returning std::nullopt if the operation would overflow.
 * @tparam T The type of the values to multiply.
 * @param a The first value.
 * @param b The seconds value.
 * @return The product of a and b, or std::nullopt if the operation would overflow.
 */
template<typename T>
[[nodiscard]] std::optional<T> mul(T a, T b) {
    static_assert(std::is_integral_v<T>, "T must be an integral type");

    // Handle zero cases early
    if (a == 0 || b == 0) {
        return T {0};
    }

    // Check for overflow or underflow before performing the multiplication
    if (a > 0) {
        if (b > 0) {
            if (a > std::numeric_limits<T>::max() / b) {
                return std::nullopt;  // Positive overflow
            }
        } else {
            if (b < std::numeric_limits<T>::min() / a) {
                return std::nullopt;  // Negative underflow
            }
        }
    } else {
        if (b > 0) {
            if (a < std::numeric_limits<T>::min() / b) {
                return std::nullopt;  // Negative underflow
            }
        } else {
            if (a < std::numeric_limits<T>::max() / b) {
                return std::nullopt;  // Positive overflow
            }
        }
    }

    return a * b;
}

/**
 * Divides a by b, returning std::nullopt if the operation would overflow or divide by zero.
 * @tparam T The type of the values to divide.
 * @param a
 * @param b
 * @return
 */
template<typename T>
[[nodiscard]] std::optional<T> div(T a, T b) {
    static_assert(std::is_integral_v<T>, "T must be an integral type");

    // Check for division by zero
    if (b == 0) {
        return std::nullopt;
    }

    // Check for overflow when dividing the smallest negative value by -1
    if constexpr (std::is_signed_v<T>) {
        if (a == std::numeric_limits<T>::min() && b == -1) {
            return std::nullopt;
        }
    }

    return a / b;
}

}  // namespace rav::safe_math
