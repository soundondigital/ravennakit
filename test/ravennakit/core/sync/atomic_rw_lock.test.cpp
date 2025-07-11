/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/sync/atomic_rw_lock.hpp"
#include "ravennakit/core/util/exclusive_access_guard.hpp"

#include <catch2/catch_all.hpp>

#include <future>
#include <thread>
#include <boost/mpl/integral_c_tag.hpp>

static_assert(!std::is_copy_constructible_v<rav::AtomicRwLock>);
static_assert(!std::is_move_constructible_v<rav::AtomicRwLock>);
static_assert(!std::is_copy_assignable_v<rav::AtomicRwLock>);
static_assert(!std::is_move_assignable_v<rav::AtomicRwLock>);

TEST_CASE("rav::AtomicRwLock") {
    SECTION("Test basic operation") {
        rav::AtomicRwLock lock;

        {
            auto guard = lock.lock_exclusive();
            REQUIRE(guard);

            auto guard2 = lock.try_lock_shared();
            REQUIRE_FALSE(guard2);

            auto guard3 = lock.try_lock_exclusive();
            REQUIRE_FALSE(guard3);
        }

        {
            auto guard = lock.lock_shared();
            REQUIRE(guard);

            auto guard2 = lock.lock_shared();
            REQUIRE(guard2);

            auto guard3 = lock.try_lock_shared();
            REQUIRE(guard3);

            auto guard4 = lock.try_lock_exclusive();
            REQUIRE_FALSE(guard4);
        }

        auto guard = lock.lock_exclusive();
        REQUIRE(guard);
    }

    SECTION("Multiple writers, multiple readers") {
        rav::AtomicRwLock lock;

        std::atomic_bool error {};
        std::atomic<int8_t> exclusive_counter {};

        std::vector<std::thread> readers;
        readers.reserve(10);
        for (size_t i = 0; i < readers.capacity(); ++i) {
            readers.emplace_back([&lock, &exclusive_counter, &error] {
                int succeeded_times = 0;
                while (succeeded_times < 10) {
                    const auto guard = lock.try_lock_shared();
                    if (!guard) {
                        continue;
                    }

                    if (exclusive_counter.fetch_add(2, std::memory_order_relaxed) % 2 != 0) {
                        // Odd which means a writer is active
                        error = true;
                        return;
                    }
                    succeeded_times++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                    if (exclusive_counter.fetch_sub(2, std::memory_order_relaxed) % 2 != 0) {
                        // Odd which means a writer is active
                        error = true;
                        return;
                    }
                }
            });
        }

        std::vector<std::thread> try_readers;
        try_readers.reserve(10);
        for (size_t i = 0; i < try_readers.capacity(); ++i) {
            try_readers.emplace_back([&lock, &error, &exclusive_counter] {
                int succeeded_times = 0;
                while (succeeded_times < 10) {
                    const auto guard = lock.lock_shared();
                    if (!guard) {
                        error = true;
                        return;
                    }

                    if (exclusive_counter.fetch_add(2, std::memory_order_relaxed) % 2 != 0) {
                        // Odd which means a writer is active
                        error = true;
                        return;
                    }
                    succeeded_times++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(15));
                    if (exclusive_counter.fetch_sub(2, std::memory_order_relaxed) % 2 != 0) {
                        // Odd which means a writer is active
                        error = true;
                        return;
                    }
                }
            });
        }

        // Writers
        std::vector<std::thread> writers;
        writers.reserve(10);
        for (size_t i = 0; i < writers.capacity(); ++i) {
            writers.emplace_back([&lock, &error, &exclusive_counter] {
                int succeeded_times = 0;
                while (succeeded_times < 10) {
                    const auto guard = lock.lock_exclusive();
                    if (!guard) {
                        error = true;
                        return;
                    }
                    if (exclusive_counter.fetch_add(1, std::memory_order_relaxed) > 0) {
                        error = true;
                        return;
                    }
                    succeeded_times++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    if (exclusive_counter.fetch_sub(1, std::memory_order_relaxed) != 1) {
                        error = true;
                        return;
                    };
                }
            });
        }

        for (auto& writer : writers) {
            writer.join();
        }

        for (auto& reader : readers) {
            reader.join();
        }

        for (auto& reader : try_readers) {
            reader.join();
        }

        REQUIRE_FALSE(error);
    }
}
