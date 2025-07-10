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

#include <catch2/catch_all.hpp>

#include <future>
#include <thread>

static_assert(!std::is_copy_constructible_v<rav::AtomicRwLock>);
static_assert(!std::is_move_constructible_v<rav::AtomicRwLock>);
static_assert(!std::is_copy_assignable_v<rav::AtomicRwLock>);
static_assert(!std::is_move_assignable_v<rav::AtomicRwLock>);

TEST_CASE("rav::AtomicRwLock") {
    SECTION("Test basic operation") {
        rav::AtomicRwLock lock;

        REQUIRE(lock.lock_exclusive());
        REQUIRE_FALSE(lock.try_lock_shared());
        REQUIRE_FALSE(lock.try_lock_exclusive());
        lock.unlock_exclusive();

        REQUIRE(lock.lock_shared());
        REQUIRE(lock.lock_shared());
        REQUIRE(lock.try_lock_shared());

        REQUIRE_FALSE(lock.try_lock_exclusive());

        lock.unlock_shared();
        lock.unlock_shared();
        lock.unlock_shared();

        REQUIRE(lock.lock_exclusive());
        lock.unlock_exclusive();
    }

    SECTION("Single write, multiple readers") {
        rav::AtomicRwLock lock;

        std::atomic_bool error {};

        static constexpr size_t k_max_value = 200;
        size_t value = 0;

        std::thread writer([&lock, &error, &value]() {
            for (size_t i = 0; i < k_max_value; ++i) {
                if (!lock.lock_exclusive()) {
                    error = true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                value++;
                lock.unlock_exclusive();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        std::vector<std::thread> readers;

        // Try locks
        for (int i = 0; i < 10; ++i) {
            readers.emplace_back([&lock, &value] {
                while (true) {
                    size_t read_value {};
                    if (lock.try_lock_shared()) {
                        read_value = value;
                        lock.unlock_shared();
                    }
                    if (read_value == k_max_value) {
                        break;
                    }
                    std::this_thread::yield();
                }
            });
        }

        // Locks
        for (int i = 0; i < 10; ++i) {
            readers.emplace_back([&lock, &value, &error] {
                while (true) {
                    size_t read_value {};
                    if (!lock.lock_shared()) {
                        error = true;
                    }
                    read_value = value;
                    lock.unlock_shared();
                    if (read_value == k_max_value) {
                        break;
                    }
                }
            });
        }

        writer.join();

        for (auto& reader : readers) {
            reader.join();
        }

        REQUIRE_FALSE(error);
    }
}
