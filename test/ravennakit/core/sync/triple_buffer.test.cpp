/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/sync/triple_buffer.hpp"

#include <thread>
#include <catch2/catch_all.hpp>

TEST_CASE("TripleBuffer") {
    SECTION("Basic operation") {
        rav::TripleBuffer<int> buffer;
        REQUIRE(buffer.get() == std::nullopt);
        buffer.update(42);
        REQUIRE(buffer.get() == 42);
        REQUIRE(buffer.get() == std::nullopt);
        buffer.update(43);
        buffer.update(44);
        REQUIRE(buffer.get() == 44);
        REQUIRE(buffer.get() == std::nullopt);
    }

    SECTION("Equal speed") {
        static constexpr int k_num_iterations = 5000;

        rav::TripleBuffer<int> buffer;

        std::thread producer([&] {
            for (int i = 0; i < k_num_iterations; ++i) {
                buffer.update(i);
            }
        });

        std::thread consumer([&] {
            int prev = -1;
            for (int i = 0; i < k_num_iterations; ++i) {
                if (auto value = buffer.get()) {
                    if (value <= prev) {
                        throw std::runtime_error("Out of order value");
                    }
                    prev = *value;
                }
            }
        });

        producer.join();
        consumer.join();
    }

    SECTION("Faster producer") {
        static constexpr int k_num_iterations = 5000;

        rav::TripleBuffer<int> buffer;

        std::thread producer([&] {
            for (int i = 0; i < k_num_iterations; ++i) {
                buffer.update(i);
            }
        });

        std::thread consumer([&] {
            int prev = -1;
            for (int i = 0; i < k_num_iterations; ++i) {
                if (auto value = buffer.get()) {
                    if (value <= prev) {
                        throw std::runtime_error("Out of order value");
                    }
                    prev = *value;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });

        producer.join();
        consumer.join();
    }

    SECTION("Faster consumer") {
        static constexpr int k_num_iterations = 5000;

        rav::TripleBuffer<int> buffer;

        std::thread producer([&] {
            for (int i = 0; i < k_num_iterations; ++i) {
                buffer.update(i);
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        });

        std::thread consumer([&] {
            int prev = -1;
            for (int i = 0; i < k_num_iterations; ++i) {
                if (auto value = buffer.get()) {
                    if (value <= prev) {
                        throw std::runtime_error("Out of order value");
                    }
                    prev = *value;
                }
            }
        });

        producer.join();
        consumer.join();
    }
}
