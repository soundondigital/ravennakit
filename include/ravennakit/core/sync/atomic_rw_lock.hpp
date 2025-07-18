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
 * A lock-free (and sometimes wait-free) reader writer lock.
 */
class AtomicRwLock {
  public:
    /// The max number of tries before giving up (preventing runaway code).
    static constexpr size_t k_loop_upper_bound = 1'000'000;

    /// The number of iterations after which a function will start yielding.
    static constexpr uint32_t k_yield_threshold = 10;

    /// The number of iterations after which a function will start sleeping.
    static constexpr uint32_t k_sleep_threshold = 10'000;

    /// Exclusive unlock function.
    struct Exclusive {
        static void unlock(AtomicRwLock* lock) {
            lock->unlock_exclusive();
        }
    };

    /// Shared unlock function.
    struct Shared {
        static void unlock(AtomicRwLock* lock) {
            lock->unlock_shared();
        }
    };

    /// Guards access to a critical region. When this class goes out of scope it will unlock the AtomicRwLock if valid,
    /// otherwise does nothing.
    template<typename T>
    struct AccessGuard {
        explicit AccessGuard(AtomicRwLock* lock) : rw_lock(lock) {}

        ~AccessGuard() {
            if (rw_lock) {
                T::unlock(rw_lock);
            }
        }

        AccessGuard(const AccessGuard& other) = delete;
        AccessGuard& operator=(const AccessGuard& other) = delete;

        AccessGuard(AccessGuard&& other) noexcept {
            if (this != &other) {
                std::swap(rw_lock, other.rw_lock);
            }
        }

        AccessGuard& operator=(AccessGuard&& other) noexcept = delete;

        /// @returns True if this guard holds a lock, or false if it doesn't.
        explicit operator bool() const {
            return rw_lock != nullptr;
        }

      private:
        AtomicRwLock* rw_lock {};
    };

    /**
     * Attempts to acquire an exclusive lock, spinning util it succeeds, or until the loop upper bounds is reached.
     * Thread safe: yes
     * Wait-free: no
     * @return A guard which holds on to the lock as long as it's alive. Check for validity before using.
     */
    [[nodiscard]] AccessGuard<Exclusive> lock_exclusive() {
        // Try the shortcut
        if (auto guard = try_lock_exclusive()) {
            return guard;
        }

        for (size_t i = 0; i < k_loop_upper_bound; ++i) {
            uint32_t waiting_state = k_exclusive_lock_waiting_bit;
            readers.fetch_or(waiting_state, std::memory_order_release);

            if (readers.compare_exchange_strong(waiting_state, k_exclusive_lock_bit, std::memory_order_acq_rel)) {
                return AccessGuard<Exclusive>(this);
            }

            if (i >= k_sleep_threshold) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else if (i >= k_yield_threshold) {
                std::this_thread::yield();
            }
        }

        readers.fetch_and(~k_exclusive_lock_waiting_bit, std::memory_order_release);

        RAV_ERROR("Loop upper bound reached");
        return AccessGuard<Exclusive>(nullptr);
    }

    /**
     * Attempts to acquire an exclusive lock.
     * Note: this call can fail if a reader is trying to get access at the same time.
     * Thread safe: yes
     * Wait-free: yes
     * @return A guard which holds on to the lock as long as it's alive. Check for validity before using.
     */
    [[nodiscard]] AccessGuard<Exclusive> try_lock_exclusive() {
        uint32_t e = 0;
        if (readers.compare_exchange_strong(e, k_exclusive_lock_bit, std::memory_order_acq_rel)) {
            return AccessGuard<Exclusive>(this);
        }
        return AccessGuard<Exclusive>(nullptr);
    }

    /**
     * Attempts to acquire a shared lock, spinning util it succeeds, or until the loop upper bounds is reached.
     * Thread safe: yes
     * Wait-free: no
     * @return A guard which holds on to the lock as long as it's alive. Check for validity before using.
     */
    [[nodiscard]] AccessGuard<Shared> lock_shared() {
        for (size_t i = 0; i < k_loop_upper_bound; ++i) {
            if (auto guard = try_lock_shared()) {
                return guard;
            }

            if (i >= k_sleep_threshold) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } else if (i >= k_yield_threshold) {
                std::this_thread::yield();
            }
        }
        RAV_ERROR("Loop upper bound reached");
        return AccessGuard<Shared>(nullptr);
    }

    /**
     * Attempts to acquire a shared lock. This will always succeed when there is no exclusive lock.
     * This call will always succeed if there are no writers (waiting).
     * Thread safe: yes
     * Wait-free: yes
     * @return A guard which holds on to the lock as long as it's alive. Check for validity before using.
     */
    [[nodiscard]] AccessGuard<Shared> try_lock_shared() {
        uint32_t prev_readers = readers.load(std::memory_order_acquire);

        if (prev_readers >= k_readers_mask) {
            return AccessGuard<Shared>(nullptr);
        }

        prev_readers = readers.fetch_add(1, std::memory_order_acq_rel);
        if (prev_readers >= k_readers_mask) {
            readers.fetch_sub(1, std::memory_order_release);
            return AccessGuard<Shared>(nullptr);
        }

        return AccessGuard<Shared>(this);
    }

    /**
     * @return True if shared locked. This is a relaxed check and cannot be used to draw any conclusions.
     */
    [[nodiscard]] bool is_locked_shared() const {
        return (readers.load(std::memory_order_relaxed) & k_readers_mask) > 0;
    }

    /**
     * @return True if locked exclusively. This is a relaxed check and cannot be used to draw any conclusions.
     */
    [[nodiscard]] bool is_locked_exclusively() const {
        return readers.load(std::memory_order_relaxed) & k_exclusive_lock_bit;
    }

    /**
     * @return True if locked shared or exclusively. This is a relaxed check and cannot be used to draw any conclusions.
     */
    [[nodiscard]] bool is_locked() const {
        return readers.load(std::memory_order_relaxed) > 0;
    }

  private:
    static constexpr uint32_t k_exclusive_lock_bit = 1u << 30;
    static constexpr uint32_t k_exclusive_lock_waiting_bit = 1u << 31;
    static constexpr uint32_t k_readers_mask = 0xFFFFFF;
    std::atomic<uint32_t> readers {0};

    /**
     * Releases the exclusive lock. Only call this when a successful call to lock_exclusive or try_lock_exclusive was
     * made before.
     * Thread safe: only when called from a thread which holds the exclusive lock
     * Wait-free: yes
     */
    void unlock_exclusive() {
        const auto prev = readers.fetch_and(~k_exclusive_lock_bit, std::memory_order_acq_rel);
        RAV_ASSERT(prev & k_exclusive_lock_bit, "Was not locked exclusively");
    }

    /**
     * Releases the shared lock. Only call this when a successful call to lock_shared or try_lock_shared was
     * made before.
     * Thread safe: only when called from a thread which holds a shared lock
     * Wait-free: yes
     */
    void unlock_shared() {
        const auto prev = readers.fetch_sub(1, std::memory_order_acq_rel);
        RAV_ASSERT((prev & k_exclusive_lock_bit) == 0, "Is locked exclusively");
        RAV_ASSERT((prev & k_readers_mask) > 0, "Is not locked shared");
    }
};

}  // namespace rav
