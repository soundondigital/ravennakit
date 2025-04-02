/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/sync/double_buffer.hpp"

#include <catch2/catch_all.hpp>
#include <thread>

TEST_CASE("DoubleBuffer") {
    static constexpr int k_num_iterations = 500000;

    SECTION("Basic operation") {
        rav::DoubleBuffer<int> buffer;
        buffer.update(42);
        REQUIRE(buffer.get() == 42);
        REQUIRE_FALSE(buffer.get().has_value());
    }

    SECTION("Equal speed") {
        rav::DoubleBuffer<int> buffer;

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
        rav::DoubleBuffer<int> buffer;

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
        rav::DoubleBuffer<int> buffer;

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
