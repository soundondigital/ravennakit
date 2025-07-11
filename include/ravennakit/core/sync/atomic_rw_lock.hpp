/*
 * Owllab License Agreement
 *
 * This software is provided by Owllab and may not be used, copied, modified,
 * merged, published, distributed, sublicensed, or sold without a valid and
 * explicit agreement with Owllab.
 *
 * Copyright (c) 2025 Owllab. All rights reserved.
 */

#pragma once
#include "ravennakit/core/assert.hpp"

#include <atomic>
#include <cstdint>
#include <thread>

namespace rav {

/**
 * A reader writer lock around atomics,
 */
class AtomicRwLock {
  public:
    static constexpr size_t k_loop_upper_bound = 1'000'000;
    const uint32_t k_yield_threshold = 10;      // The number of iterations after which a function will start yielding
    const uint32_t k_sleep_threshold = 10'000;  // The number of iterations after which a function will start sleeping.

    struct ExclusiveAccessGuard {
        ExclusiveAccessGuard(AtomicRwLock& lock, const bool was_locked) : rw_lock(lock), holds_lock(was_locked) {}

        ~ExclusiveAccessGuard() {
            if (holds_lock) {
                rw_lock.unlock_exclusive();
            }
        }

        ExclusiveAccessGuard(const ExclusiveAccessGuard& other) = delete;
        ExclusiveAccessGuard& operator=(const ExclusiveAccessGuard& other) = delete;

        ExclusiveAccessGuard(ExclusiveAccessGuard&& other) noexcept = delete;
        ExclusiveAccessGuard& operator=(ExclusiveAccessGuard&& other) noexcept = delete;

        /// @returns True if this guard holds a lock, or false if it doesn't.
        explicit operator bool() const {
            return holds_lock;
        }

      private:
        AtomicRwLock& rw_lock;
        bool holds_lock = false;
    };

    struct SharedAccessGuard {
        SharedAccessGuard(AtomicRwLock& lock, const bool was_locked) : rw_lock(lock), holds_lock(was_locked) {}

        ~SharedAccessGuard() {
            if (holds_lock) {
                rw_lock.unlock_shared();
            }
        }

        SharedAccessGuard(const SharedAccessGuard& other) = delete;
        SharedAccessGuard& operator=(const SharedAccessGuard& other) = delete;

        SharedAccessGuard(SharedAccessGuard&& other) noexcept = delete;
        SharedAccessGuard& operator=(SharedAccessGuard&& other) noexcept = delete;

        /// @returns True if this guard holds a lock, or false if it doesn't.
        explicit operator bool() const {
            return holds_lock;
        }

      private:
        AtomicRwLock& rw_lock;
        bool holds_lock = false;
    };

    /**
     * Attempts to acquire an exclusive lock, spinning util it succeeds, or until the loop upper bounds is reached.
     * Thread safe: yes
     * Wait-free: no
     * @return True if the lock was acquired, or false if the loop upper bound was reached.
     */
    [[nodiscard]] ExclusiveAccessGuard lock_exclusive() {
        for (size_t i = 0; i < k_loop_upper_bound; ++i) {
            uint32_t prev_readers = readers.load(std::memory_order_acquire);
            if (prev_readers <= 1) {
                if (readers.compare_exchange_weak(prev_readers, std::numeric_limits<uint32_t>::max())) {
                    return {*this, true};
                }
            }

            if (prev_readers % 2 == 0) {
                // Indicate that there is a writer waiting by making reader count uneven
                readers.compare_exchange_weak(prev_readers, prev_readers + 1);
            }

            if (i >= k_sleep_threshold) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else if (i >= k_yield_threshold) {
                std::this_thread::yield();
            }
        }
        RAV_ERROR("Loop upper bound reached");
        return {*this, false};
    }

    /**
     * Attempts to acquire an exclusive lock.
     * Thread safe: yes
     * Wait-free: yes
     * @return True if the lock was acquired, or false if not.
     */
    [[nodiscard]] ExclusiveAccessGuard try_lock_exclusive() {
        uint32_t prev_readers = readers.load(std::memory_order_acquire);
        if (prev_readers <= 1) {
            if (readers.compare_exchange_strong(prev_readers, std::numeric_limits<uint32_t>::max())) {
                return {*this, true};
            }
        }
        return {*this, false};
    }

    /**
     * Attempts to acquire a shared lock, spinning util it succeeds, or until the loop upper bounds is reached.
     * Thread safe: yes
     * Wait-free: no
     * @return True if the lock was acquired, or false if the loop upper bound was reached.
     */
    [[nodiscard]] SharedAccessGuard lock_shared() {
        for (size_t i = 0; i < k_loop_upper_bound; ++i) {
            uint32_t prev_readers = readers.load(std::memory_order_acquire);

            if (prev_readers % 2 == 0 && prev_readers < std::numeric_limits<uint32_t>::max()) {
                if (prev_readers >= std::numeric_limits<uint32_t>::max() - 2) {
                    RAV_ERROR("Too many readers");
                    return {*this, false};
                }
                if (readers.compare_exchange_weak(prev_readers, prev_readers + 2)) {
                    return {*this, true};
                }
            }

            if (i >= k_sleep_threshold) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else if (i >= k_yield_threshold) {
                std::this_thread::yield();
            }
        }
        RAV_ERROR("Loop upper bound reached");
        return {*this, false};
    }

    /**
     * Attempts to acquire a shared lock.
     * Thread safe: yes
     * Wait-free: yes
     * @return True if the lock was acquired, or false if not.
     */
    [[nodiscard]] SharedAccessGuard try_lock_shared() {
        uint32_t prev_readers = readers.load(std::memory_order_acquire);

        if (prev_readers % 2 == 0 && prev_readers < std::numeric_limits<uint32_t>::max()) {
            if (prev_readers >= std::numeric_limits<uint32_t>::max() - 2) {
                RAV_ERROR("Too many readers");
                return {*this, false};
            }
            if (readers.compare_exchange_weak(prev_readers, prev_readers + 2)) {
                return {*this, true};
            }
        }
        return {*this, false};
    }

  private:
    std::atomic<uint32_t> readers {0};

    /**
     * Releases the exclusive lock. Only call this when a successful call to lock_exclusive or try_lock_exclusive was
     * made before.
     * Thread safe: only when called from a thread which holds the exclusive lock
     * Wait-free: yes
     */
    void unlock_exclusive() {
        const uint32_t prev_readers = readers.load(std::memory_order_acquire);
        if (prev_readers != std::numeric_limits<uint32_t>::max()) {
            RAV_ASSERT_FALSE("Not exclusively locked");
            return;
        }
        readers.store(0, std::memory_order_release);
    }

    /**
     * Releases the shared lock. Only call this when a successful call to lock_shared or try_lock_shared was
     * made before.
     * Thread safe: only when called from a thread which holds a shared lock
     * Wait-free: no
     */
    void unlock_shared() {
        const uint32_t prev_readers = readers.load(std::memory_order_acquire);
        if (prev_readers == std::numeric_limits<uint32_t>::max() || prev_readers == 0) {
            RAV_ASSERT_FALSE("Not shared locked");
            return;
        }
        readers.fetch_sub(2, std::memory_order_release);
    }
};

}  // namespace rav
