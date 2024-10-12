/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2024 Owllab. All rights reserved.
 */

#include "ravennakit/util/exclusive_access_guard.hpp"

#include <thread>
#include <catch2/catch_all.hpp>

namespace {

void violates_exclusive_access() {
    std::atomic counter {0};
    const rav::exclusive_access_guard exclusive_access_guard1(counter);
    const rav::exclusive_access_guard exclusive_access_guard2(counter);
}

void exclusive_access() {
    static std::atomic counter {0};
    const rav::exclusive_access_guard exclusive_access_guard(counter);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));  // Introduce a delay to increase the chance
}

}  // namespace

TEST_CASE("exclusive_access_guard", "[exclusive_access_guard]") {
    SECTION("Exclusive access violation") {
        REQUIRE_THROWS(violates_exclusive_access());
    }

    SECTION("Trigger exclusive access violation by running two threads") {
        std::atomic keep_going {true};

        auto thread_function = [&] {
            while (keep_going) {
                try {
                    exclusive_access();
                } catch (const std::exception&) {
                    keep_going = false;
                }
            }
        };

        std::thread thread1(thread_function);
        std::thread thread2(thread_function);

        thread1.join();
        thread2.join();
    }
}
