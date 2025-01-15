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
#include <cmath>
#include <cstddef>

namespace rav::util {

/**
 * Returns the number of elements in a c-style array.
 * @tparam Type The type of the elements.
 * @tparam N The total number of elements.
 * @return The number of elements in the array.
 */
template<typename Type, size_t N>
constexpr size_t num_elements_in_array(Type (&)[N]) noexcept {
    return N;
}

/**
 * Tests if two values are within a certain tolerance of each other.
 * @tparam T The type of the values to compare.
 * @param a The first value.
 * @param b The second value.
 * @param tolerance The tolerance.
 * @return True if the values are within the tolerance of each other, false otherwise.
 */
template<typename T>
bool is_within(T a, T b, T tolerance) {
    return std::fabs(a - b) <= tolerance;
}

/**
 * Tests if a value is between two other values, inclusive.
 * @tparam T The type of the values to compare.
 * @param a The value to test.
 * @param min The minimum value.
 * @param max The maximum value.
 * @return True if the value is between the minimum and maximum values, false otherwise.
 */
template<typename T>
bool is_between(T a, T min, T max) {
    return a >= min && a <= max;
}

}  // namespace rav::util
