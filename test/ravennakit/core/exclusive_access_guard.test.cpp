/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/core/util/exclusive_access_guard.hpp"

#include <future>
#include <thread>
#include <catch2/catch_all.hpp>

TEST_CASE("rav::ExclusiveAccessGuard") {
    SECTION("Exclusive access violation") {
        rav::ExclusiveAccessGuard guard;

        const rav::ExclusiveAccessGuard::Lock lock1(guard);
        const rav::ExclusiveAccessGuard::Lock lock2(guard);

        REQUIRE_FALSE(lock1.violated());
        REQUIRE(lock2.violated());
    }

    SECTION("Trigger exclusive access violation by running two threads") {
        std::atomic keep_going {true};

        rav::ExclusiveAccessGuard guard;

        auto function = [&keep_going, &guard]() {
            while (keep_going) {
                const rav::ExclusiveAccessGuard::Lock lock(guard);
                if (lock.violated()) {
                    keep_going = false;
                    return true;
                }
                // Introduce a delay to increase the chance of violation
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            return false;
        };

        auto f1 = std::async(std::launch::async, function);
        auto f2 = std::async(std::launch::async, function);

        f1.wait_for(std::chrono::seconds(5));
        f2.wait_for(std::chrono::seconds(5));

        REQUIRE((f1.get() || f2.get()));
    }
}
