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

#include <random>
#include <string>

namespace rav::random {

/**
 * Generates alphanumeric random string.
 * @param length The length of the string to generate.
 * @return The pseudo randomly generated string
 */
inline std::string generate_random_string(const size_t length) {
    const std::string charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution dist(0, static_cast<int>(charset.size()) - 1);

    std::string result(length, 0);
    std::generate_n(result.begin(), length, [&]() {
        return charset[static_cast<size_t>(dist(generator))];
    });
    return result;
}

}  // namespace rav::random
