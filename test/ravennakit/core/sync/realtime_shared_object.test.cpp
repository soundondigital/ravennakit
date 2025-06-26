/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#include "ravennakit/core/sync/realtime_shared_object.hpp"

#include <future>
#include <catch2/catch_all.hpp>

static_assert(!std::is_copy_constructible_v<rav::RealtimeSharedObject<int>>);
static_assert(!std::is_move_constructible_v<rav::RealtimeSharedObject<int>>);
static_assert(!std::is_copy_assignable_v<rav::RealtimeSharedObject<int>>);
static_assert(!std::is_move_assignable_v<rav::RealtimeSharedObject<int>>);

static_assert(!std::is_copy_constructible_v<rav::RealtimeSharedObject<int>::RealtimeLock>);
static_assert(!std::is_move_constructible_v<rav::RealtimeSharedObject<int>::RealtimeLock>);
static_assert(!std::is_copy_assignable_v<rav::RealtimeSharedObject<int>::RealtimeLock>);
static_assert(!std::is_move_assignable_v<rav::RealtimeSharedObject<int>::RealtimeLock>);

TEST_CASE("rav::RealtimeSharedObject") {
    SECTION("Default state") {
        rav::RealtimeSharedObject<std::string> obj;
        auto lock = obj.lock_realtime();
        REQUIRE(lock.get() != nullptr);
        REQUIRE(lock->empty());
    }

    SECTION("Updating and reading the value should be thread safe") {
        static constexpr size_t num_values = 50;
        static constexpr size_t num_writer_threads = 2;

        rav::RealtimeSharedObject<std::pair<size_t, std::string>> obj;

        std::atomic_bool keep_going {true};

        auto reader = std::async(std::launch::async, [&obj, &keep_going] {
            size_t num_values_read = 0;
            std::vector<std::string> values(num_values);

            while (num_values_read < num_values) {
                const auto lock = obj.lock_realtime();
                if (lock.get() == nullptr) {
                    return std::vector<std::string>();
                }
                if (lock->second.empty()) {
                    continue;  // obj was default constructed
                }
                if (lock->first >= num_values) {
                    return std::vector<std::string>();  // obj was updated with an invalid index
                }
                auto& it = values.at(lock->first);
                if (it.empty()) {
                    it = lock->second;
                    ++num_values_read;
                }
            }

            keep_going = false;
            return values;
        });

        // Give reader thread some time to start.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Create writer threads.
        std::vector<std::thread> writer_threads;
        for (size_t i = 0; i < num_writer_threads; ++i) {
            writer_threads.emplace_back([&obj, &keep_going] {
                // Writers are going to hammer the object with new values until the reader has read all values.
                while (keep_going) {
                    for (size_t j = 0; j < num_values; ++j) {
                        std::ignore = obj.update(j, std::to_string(j + 1));
                        std::this_thread::yield();
                    }
                }
            });
        }

        auto read_values = reader.get();

        for (auto& thread : writer_threads) {
            thread.join();
        }

        for (size_t i = 0; i < num_values; ++i) {
            REQUIRE(read_values[i] == std::to_string(i + 1));
        }
    }
}
