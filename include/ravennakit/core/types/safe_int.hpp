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

#include "ravennakit/core/expected.hpp"

#include <limits>

namespace rav {

enum class SafeIntError { overflow, underflow, div_by_zero };

/**
 * A safe integer type that checks for overflow and underflow when performing arithmetic operations.
 * @tparam T The type of the integer.
 */
template<class T>
class SafeInt {
  public:
    static_assert(std::is_integral_v<T>, "T must be an integral type");

    SafeInt() = default;

    /**
     * Constructs a safe_int from a value.
     * @param value The value to store.
     */
    explicit SafeInt(T value) : value_ {value} {}

    /**
     * @return The value stored in the safe_int.
     * @throws When the value is in an error state.
     */
    T value() const {
        return value_.value();
    }

    /**
     * @return The error state of the safe_int.
     * @throws When the value is not in an error state.
     */
    [[nodiscard]] SafeIntError error() const {
        return value_.error();
    }

    /**
     * @return The value stored in the safe_int.
     */
    tl::expected<T, SafeIntError> expected() const {
        return value_;
    }

    /**
     * Adds a value to the safe_int, returning an error if the operation would overflow or underflow.
     * @tparam U The type of the value to add.
     * @param b The value to add.
     * @return A reference to the safe_int.
     */
    template<class U>
    SafeInt& operator+=(U b) {
        if (!value_) {
            return *this;
        }
        value_ = add(*value_, b);
        return *this;
    }

    /**
     * Adds a value to the safe_int, returning a new safe_int with the result.
     * @tparam U The type of the value to add.
     * @param b The value to add.
     * @return A new safe_int with the result.
     */
    template<class U>
    SafeInt operator+(U b) const {
        SafeInt r = *this;
        r += b;
        return r;
    }

    /**
     * Subtracts a value from the safe_int, returning an error if the operation would overflow or underflow.
     * @tparam U The type of the value to subtract.
     * @param b The value to subtract.
     * @return A reference to the safe_int.
     */
    template<class U>
    SafeInt& operator-=(U b) {
        if (!value_) {
            return *this;
        }
        value_ = sub(*value_, b);
        return *this;
    }

    /**
     * Subtracts a value from the safe_int, returning a new safe_int with the result.
     * @tparam U The type of the value to subtract.
     * @param b The value to subtract.
     * @return A new safe_int with the result.
     */
    template<class U>
    SafeInt operator-(U b) const {
        SafeInt r = *this;
        r -= b;
        return r;
    }

    /**
     * Multiplies the safe_int by a scalar, returning an error if the operation would overflow or underflow.
     * @tparam U The type of the scalar.
     * @param b The scalar to multiply by.
     * @return A reference to the safe_int.
     */
    template<class U>
    SafeInt& operator*=(U b) {
        if (!value_) {
            return *this;
        }
        value_ = mul(*value_, b);
        return *this;
    }

    /**
     * Multiplies the safe_int by a scalar, returning a new safe_int with the result.
     * @tparam U The type of the scalar.
     * @param b The scalar to multiply by.
     * @return A new safe_int with the result.
     */
    template<class U>
    SafeInt operator*(U b) const {
        SafeInt r = *this;
        r *= b;
        return r;
    }

    /**
     * Divides the safe_int by a scalar, returning an error if the operation would overflow or underflow.
     * @tparam U The type of the scalar.
     * @param b The scalar to divide by.
     * @return A reference to the safe_int.
     */
    template<class U>
    SafeInt& operator/=(U b) {
        if (!value_) {
            return *this;
        }
        value_ = div(*value_, b);
        return *this;
    }

    /**
     * Divides the safe_int by a scalar, returning a new safe_int with the result.
     * @tparam U The type of the scalar.
     * @param b The scalar to divide by.
     * @return A new safe_int with the result.
     */
    template<class U>
    SafeInt operator/(U b) const {
        SafeInt r = *this;
        r /= b;
        return r;
    }

    /**
     * Adds two values, returning an error if the operation would overflow or underflow.
     * @tparam U The type of the values to add.
     * @param a The first value.
     * @param b The second value.
     * @return The sum of the two values, or an error if the operation would overflow or underflow.
     */
    template<class U>
    static tl::expected<T, SafeIntError> add(T a, U b) {
        if (b > 0 && a > std::numeric_limits<T>::max() - b) {
            return tl::unexpected(SafeIntError::overflow);
        }
        if (b < 0 && a < std::numeric_limits<T>::min() - b) {
            return tl::unexpected(SafeIntError::underflow);
        }
        return a + b;
    }

    /**
     * Subtracts two values, returning an error if the operation would overflow or underflow.
     * @tparam U The type of the values to subtract.
     * @param a The first value.
     * @param b The second value.
     * @return The difference of the two values, or an error if the operation would overflow or underflow.
     */
    template<class U>
    static tl::expected<T, SafeIntError> sub(T a, U b) {
        if (b < 0 && a > std::numeric_limits<T>::max() + b) {
            return tl::unexpected(SafeIntError::overflow);
        }
        if (b > 0 && a < std::numeric_limits<T>::min() + b) {
            return tl::unexpected(SafeIntError::underflow);
        }
        return a - b;
    }

    /**
     * Multiplies two values, returning an error if the operation would overflow or underflow.
     * @tparam U The type of the values to multiply.
     * @param a The first value.
     * @param b The second value.
     * @return The product of the two values, or an error if the operation would overflow or underflow.
     */
    template<class U>
    static tl::expected<T, SafeIntError> mul(T a, U b) {
        // Handle zero cases early
        if (a == 0 || b == 0) {
            return T {0};
        }

        // Check for overflow or underflow before performing the multiplication
        if (a > 0) {
            if (b > 0) {
                if (a > std::numeric_limits<T>::max() / b) {
                    return tl::unexpected(SafeIntError::overflow);
                }
            } else {
                if (b < std::numeric_limits<T>::min() / a) {
                    return tl::unexpected(SafeIntError::underflow);
                }
            }
        } else {
            if (b > 0) {
                if (a < std::numeric_limits<T>::min() / b) {
                    return tl::unexpected(SafeIntError::underflow);
                }
            } else {
                if (a < std::numeric_limits<T>::max() / b) {
                    return tl::unexpected(SafeIntError::overflow);
                }
            }
        }

        return a * b;
    }

    /**
     * Divides two values, returning an error if the operation would overflow or underflow.
     * @tparam U The type of the values to divide.
     * @param a The first value.
     * @param b The second value.
     * @return The quotient of the two values, or an error if the operation would overflow, underflow, or divide by
     * zero.
     */
    template<class U>
    static tl::expected<T, SafeIntError> div(T a, U b) {
        // Check for division by zero
        if (b == 0) {
            return tl::unexpected(SafeIntError::div_by_zero);
        }

        // Check for overflow when dividing the smallest negative value by -1
        if constexpr (std::is_signed_v<T>) {
            if (a == std::numeric_limits<T>::min() && b == -1) {
                return tl::unexpected(SafeIntError::overflow);
            }
        }

        return a / b;
    }

  private:
    tl::expected<T, SafeIntError> value_ {};
};

// Convenience aliases
using SafeInt8 = SafeInt<int8_t>;
using SafeInt16 = SafeInt<int16_t>;
using SafeInt32 = SafeInt<int32_t>;
using SafeInt64 = SafeInt<int64_t>;
using SafeUint8 = SafeInt<uint8_t>;
using SafeUint16 = SafeInt<uint16_t>;
using SafeUint32 = SafeInt<uint32_t>;
using SafeUint64 = SafeInt<uint64_t>;

}  // namespace rav
