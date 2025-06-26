/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/util/safe_function.hpp"

#include <catch2/catch_all.hpp>

TEST_CASE("rav::SafeFunction") {
    rav::SafeFunction<void(const std::string& a, const std::string& b)> callback_function;
    callback_function("a", "b");
    int times_called = 0;
    callback_function.set([&](const std::string& a, const std::string& b) {
        REQUIRE(a == "a");
        REQUIRE(b == "b");
        times_called++;
    });
    callback_function("a", "b");
    REQUIRE(times_called == 1);
    callback_function.set(nullptr);
    callback_function("a", "b");
    REQUIRE(times_called == 1);
    callback_function = [&](const std::string& a, const std::string& b) {
        REQUIRE(a == "c");
        REQUIRE(b == "d");
        times_called++;
    };
    callback_function("c", "d");
    REQUIRE(times_called == 2);
}
