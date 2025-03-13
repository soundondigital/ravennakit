/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/sync/rcu.hpp"
#include "ravennakit/core/util/object_counter.hpp"

#include <future>
#include <thread>
#include <catch2/catch_all.hpp>

static_assert(!std::is_copy_constructible_v<rav::rcu<int>>);
static_assert(!std::is_move_constructible_v<rav::rcu<int>>);
static_assert(!std::is_copy_assignable_v<rav::rcu<int>>);
static_assert(!std::is_move_assignable_v<rav::rcu<int>>);

static_assert(!std::is_copy_constructible_v<rav::rcu<int>::reader>);
static_assert(!std::is_move_constructible_v<rav::rcu<int>::reader>);
static_assert(!std::is_copy_assignable_v<rav::rcu<int>::reader>);
static_assert(!std::is_move_assignable_v<rav::rcu<int>::reader>);

static_assert(!std::is_copy_constructible_v<rav::rcu<int>::reader::realtime_lock>);
static_assert(!std::is_move_constructible_v<rav::rcu<int>::reader::realtime_lock>);
static_assert(!std::is_copy_assignable_v<rav::rcu<int>::reader::realtime_lock>);
static_assert(!std::is_move_assignable_v<rav::rcu<int>::reader::realtime_lock>);

namespace {
constexpr auto k_timeout_seconds = 60;
}

TEST_CASE("rcu") {
    SECTION("Default state") {
        rav::rcu<int> rcu;
        rav::rcu<int>::reader reader(rcu);

        const auto lock = reader.lock_realtime();
        REQUIRE(lock.get() == nullptr);
    }

    SECTION("Basic operation") {
        rav::rcu<std::string> rcu;
        rav::rcu<std::string>::reader reader(rcu);

        {
            const auto lock = reader.lock_realtime();
            REQUIRE(lock.get() == nullptr);

            rcu.update("Hello, World!");

            // As long as the first lock is alive, the value won't be updated for subsequent locks of the same reader.
            const auto lock2 = reader.lock_realtime();
            REQUIRE(lock.get() == nullptr);
        }

        // Once the previous locks are destroyed, new locks will get the updated value.
        const auto lock3 = reader.lock_realtime();
        REQUIRE(*lock3 == "Hello, World!");

        // Additional locks will get the same value.
        const auto lock4 = reader.lock_realtime();
        REQUIRE(lock3.get() == lock4.get());
    }

    SECTION("Track object lifetime") {
        rav::object_counter counter;

        rav::rcu<rav::counted_object> rcu;
        rcu.update(counter);

        REQUIRE(counter.instances_created == 1);
        REQUIRE(counter.instances_alive == 1);

        rcu.update(counter);

        REQUIRE(counter.instances_created == 2);
        REQUIRE(counter.instances_alive == 2);

        REQUIRE(rcu.reclaim() == 1);

        REQUIRE(counter.instances_created == 2);
        REQUIRE(counter.instances_alive == 1);

        auto reader = rcu.create_reader();
        {
            const auto lock1 = reader.lock_realtime();
            REQUIRE(lock1.get() != nullptr);
            REQUIRE(lock1->index() == 1);
        }

        rcu.update(counter);

        REQUIRE(counter.instances_created == 3);
        REQUIRE(counter.instances_alive == 2);

        {
            const auto lock2 = reader.lock_realtime();
            REQUIRE(lock2.get() != nullptr);
            REQUIRE(lock2->index() == 2);

            rcu.update(counter);

            REQUIRE(lock2.get() != nullptr);
            REQUIRE(lock2->index() == 2);
        }

        const auto lock3 = reader.lock_realtime();
        REQUIRE(lock3.get() != nullptr);
        REQUIRE(lock3->index() == 3);

        REQUIRE(rcu.reclaim() == 2);

        REQUIRE(counter.instances_created == 4);
        REQUIRE(counter.instances_alive == 1);
    }

    SECTION("The value can be cleared") {
        rav::object_counter counter;

        rav::rcu<rav::counted_object> rcu;
        auto reader = rcu.create_reader();
        rcu.update(counter);

        REQUIRE(counter.instances_created == 1);
        REQUIRE(counter.instances_alive == 1);

        {
            auto lock = reader.lock_realtime();
            REQUIRE(lock.get() != nullptr);
            REQUIRE(lock->index() == 0);
        }

        rcu.clear();
        REQUIRE(rcu.reclaim() == 1);

        REQUIRE(counter.instances_created == 1);
        REQUIRE(counter.instances_alive == 0);

        {
            auto lock = reader.lock_realtime();
            REQUIRE(lock.get() == nullptr);
        }
    }

    SECTION("Reclaim") {
        rav::object_counter counter;
        rav::rcu<rav::counted_object> rcu;

        REQUIRE(counter.instances_created == 0);
        REQUIRE(counter.instances_alive == 0);

        rcu.update(counter);

        REQUIRE(counter.instances_created == 1);
        REQUIRE(counter.instances_alive == 1);

        // The last value should never be reclaimed
        REQUIRE(rcu.reclaim() == 0);

        REQUIRE(counter.instances_created == 1);
        REQUIRE(counter.instances_alive == 1);

        rcu.update(counter);

        REQUIRE(counter.instances_created == 2);
        REQUIRE(counter.instances_alive == 2);

        REQUIRE(rcu.reclaim() == 1);

        REQUIRE(counter.instances_created == 2);
        REQUIRE(counter.instances_alive == 1);
    }

    SECTION("Only objects older than the first object used by any reader are deleted") {
        rav::object_counter counter;
        rav::rcu<rav::counted_object> rcu;
        rcu.update(counter);

        auto reader1 = rcu.create_reader();
        auto reader2 = rcu.create_reader();

        auto reader1_lock = reader1.lock_realtime();
        REQUIRE(reader1_lock.get() != nullptr);
        REQUIRE(reader1_lock->index() == 0);

        rcu.update(counter);
        rcu.update(counter);

        auto reader2_lock = reader2.lock_realtime();
        REQUIRE(reader2_lock.get() != nullptr);
        REQUIRE(reader2_lock->index() == 2);

        REQUIRE(counter.instances_created == 3);
        REQUIRE(counter.instances_alive == 3);

        REQUIRE(rcu.reclaim() == 0);

        // Because reader1_lock is still active, no values should be deleted. Not even the 2nd one (which is not in use
        // currently).
        REQUIRE(counter.instances_created == 3);
        REQUIRE(counter.instances_alive == 3);

        reader1_lock.reset();
        REQUIRE(rcu.reclaim() == 2);

        // Now that reader1_lock has been reset, the first 2 objects can be deleted.
        REQUIRE(counter.instances_created == 3);
        REQUIRE(counter.instances_alive == 1);
    }

    SECTION("Reader does not block writer") {
        rav::rcu<std::string> rcu;
        rcu.update("Hello, World!");

        std::promise<void> value_updated;

        std::promise<void> has_read_lock;
        auto has_read_lock_future = has_read_lock.get_future();
        auto reader_thread = std::thread([&rcu, &has_read_lock, value_updated_future = value_updated.get_future()] {
            auto reader = rcu.create_reader();

            {
                auto lock = reader.lock_realtime();
                REQUIRE(lock.get() != nullptr);
                REQUIRE(*lock == "Hello, World!");

                has_read_lock.set_value();

                value_updated_future.wait();

                // We should still read the initial value since the never reset the lock
                REQUIRE(*lock == "Hello, World!");
            }

            auto lock = reader.lock_realtime();
            REQUIRE(lock.get() != nullptr);
            REQUIRE(*lock == "Updated value");
        });

        has_read_lock_future.wait();

        auto writer_thread = std::thread([&rcu, &value_updated]() mutable {
            rcu.update("Updated value");
            value_updated.set_value();
        });

        reader_thread.join();
        writer_thread.join();
    }

    SECTION("Readers can be created and destroyed concurrently") {
        rav::rcu<std::string> rcu("Hello, World!");
        static constexpr size_t num_threads = 100;
        std::vector<std::string> results(num_threads);

        std::atomic keep_going = true;
        std::atomic<size_t> num_active_threads = 0;

        std::vector<std::thread> threads;
        threads.reserve(num_threads);

        for (size_t i = 0; i < num_threads; ++i) {
            threads.emplace_back([&] {
                const auto thread_id = num_active_threads.fetch_add(1);
                while (keep_going) {
                    auto reader = rcu.create_reader();
                    auto lock = reader.lock_realtime();
                    results[thread_id] = *lock;
                }
            });
        }

        auto start = std::chrono::steady_clock::now();

        while (num_active_threads < num_threads) {
            if (start + std::chrono::seconds(k_timeout_seconds) < std::chrono::steady_clock::now()) {
                FAIL("Timeout");
            }
            std::this_thread::yield();
        }

        // Once all threads are active, keep going for another small amount of time
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        keep_going = false;

        for (auto& thread : threads) {
            thread.join();
        }

        for (auto& result : results) {
            REQUIRE(result == "Hello, World!");
        }
    }

    SECTION("Concurrent reads and writes and reclaims should be thread safe") {
        static constexpr size_t num_values = 10'000;
        static constexpr size_t num_writer_threads = 3;
        static constexpr size_t num_reader_threads = 3;
        static constexpr size_t num_reclaim_thread = 3;

        // Assign the values we're going to give the rcu object
        rav::rcu<std::pair<size_t, std::string>> rcu;

        std::atomic<size_t> num_readers_finished = 0;

        std::vector<std::thread> writer_threads;
        writer_threads.reserve(num_writer_threads);

        // Writers are going to hammer the rcu object with new values until all readers have read all values.
        for (size_t i = 0; i < num_writer_threads; ++i) {
            writer_threads.emplace_back([&num_readers_finished, &rcu] {
                while (num_readers_finished < num_reader_threads) {
                    for (size_t j = 0; j < num_values; ++j) {
                        rcu.update(std::make_pair(j, std::to_string(j + 1)));
                        std::ignore = rcu.reclaim();
                        std::this_thread::yield();
                    }
                }
            });
        }

        std::vector<std::vector<std::string>> reader_values(num_reader_threads);

        std::vector<std::thread> reader_threads;
        reader_threads.reserve(num_reader_threads);

        // Readers are going to read from the rcu until they have received all values.
        for (size_t i = 0; i < num_reader_threads; ++i) {
            reader_threads.emplace_back([&reader_values, &rcu, i, &num_readers_finished] {
                size_t num_values_read = 0;
                std::vector<std::string> output_values(num_values);

                auto reader = rcu.create_reader();

                while (num_values_read < num_values) {
                    const auto lock = reader.lock_realtime();
                    if (lock.get() == nullptr) {
                        continue;
                    }
                    auto& it = output_values.at(lock->first);
                    if (it.empty()) {
                        it = lock->second;
                        ++num_values_read;
                    }
                }

                reader_values[i] = output_values;
                num_readers_finished.fetch_add(1);
            });
        }

        // These threads are going to reclaim.
        std::vector<std::thread> reclaim_threads;
        reclaim_threads.reserve(num_reclaim_thread);

        for (size_t i = 0; i < num_reclaim_thread; ++i) {
            reclaim_threads.emplace_back([&] {
                while (num_readers_finished < num_reader_threads) {
                    std::ignore = rcu.reclaim();
                    std::this_thread::yield();
                }
            });
        }

        for (auto& thread : writer_threads) {
            thread.join();
        }

        for (auto& thread : reader_threads) {
            thread.join();
        }

        for (auto& thread : reclaim_threads) {
            thread.join();
        }

        for (auto& reader_value : reader_values) {
            for (size_t i = 0; i < num_values; ++i) {
                REQUIRE(reader_value.at(i) == std::to_string(i + 1));
            }
        }
    }
}
