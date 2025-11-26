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

#include <memory>
#include <mutex>
#include <thread>

namespace rav {

/**
 * This class provides a real-time safe way to share objects among a single reader. The writer side is protected by a
 * mutex. Internally a double buffer and CAS loop is used to update the value in a thread-safe manner.
 * @tparam T The type of the object to share.
 */
template<class T>
class RealtimeSharedObject {
  public:
    /// The max number of tries before giving up (preventing runaway code).
    static constexpr size_t k_loop_upper_bound = 1'000'000;

    /// The number of iterations after which a function will start yielding.
    static constexpr uint32_t k_yield_threshold = 10;

    /// The number of iterations after which a function will start sleeping.
    static constexpr uint32_t k_sleep_threshold = 10'000;

    /**
     * A guard object provides access to the value. The object is used to access the value in a real-time safe way.
     */
    class RealtimeAccessGuard {
      public:
        explicit RealtimeAccessGuard(RealtimeSharedObject& owner) : owner_(&owner) {
            value_ = owner_->ptr_.exchange(nullptr);
        }

        ~RealtimeAccessGuard() {
            reset();
        }

        RealtimeAccessGuard(const RealtimeAccessGuard&) = delete;
        RealtimeAccessGuard& operator=(const RealtimeAccessGuard&) = delete;
        RealtimeAccessGuard(RealtimeAccessGuard&&) = delete;
        RealtimeAccessGuard& operator=(RealtimeAccessGuard&&) = delete;

        /**
         * @return A pointer to the contained object, or nullptr if the value is nullptr.
         */
        T* get() {
            return value_;
        }

        /**
         * @return A pointer to the contained object, or nullptr if the value is nullptr.
         */
        const T* get() const {
            return value_;
        }

        /**
         * @return A reference to the contained object. Reference is only valid if the value is not nullptr.
         */
        T& operator*() {
            RAV_ASSERT_DEBUG(value_ != nullptr, "Value is nullptr");
            return *value_;
        }

        /**
         * @return A reference to the contained object. Reference is only valid if the value is not nullptr.
         */
        const T& operator*() const {
            RAV_ASSERT_DEBUG(value_ != nullptr, "Value is nullptr");
            return *value_;
        }

        /**
         * @return A pointer to the contained object, or nullptr if the value is nullptr.
         */
        T* operator->() {
            RAV_ASSERT_DEBUG(value_ != nullptr, "Value is nullptr");
            return value_;
        }

        /**
         * @return A pointer to the contained object, or nullptr if the value is nullptr.
         */
        const T* operator->() const {
            RAV_ASSERT_DEBUG(value_ != nullptr, "Value is nullptr");
            return value_;
        }

        /**
         * Resets the lock to the initial state, releasing the value.
         */
        void reset() {
            owner_->ptr_.store(value_, std::memory_order_release);
            value_ = nullptr;
        }

      private:
        RealtimeSharedObject* owner_;
        T* value_ {nullptr};
    };

    /**
     * Default constructor.
     */
    RealtimeSharedObject() = default;

    /**
     * @param initial_value Initial value to set.
     */
    explicit RealtimeSharedObject(T initial_value) : storage_ {std::move(initial_value)} {}

    ~RealtimeSharedObject() {
        RAV_ASSERT_NO_THROW(ptr_ != nullptr, "There should be no active locks");
    }

    RealtimeSharedObject(const RealtimeSharedObject&) = delete;
    RealtimeSharedObject& operator=(const RealtimeSharedObject&) = delete;
    RealtimeSharedObject(RealtimeSharedObject&&) = delete;
    RealtimeSharedObject& operator=(RealtimeSharedObject&&) = delete;

    /**
     * @return Get a lock for realtime access to the current value. During the lifetime of this lock (or until reset is
     * called) no updates can be made to the value.
     * Real-time safe: yes, wait-free
     * Thread safe: no.
     */
    [[nodiscard]] RealtimeAccessGuard access_realtime() {
        return RealtimeAccessGuard(*this);
    }

    /**
     * Swaps current value by given value by placing the new value into the active slot, and then swapping the pointer
     * for the realtime thread.
     * @param value The new value to share with the realtime thread.
     * @return An optional containing the previous value, or nullopt when the loop upper bound is reached.
     */
    [[nodiscard]] std::optional<T> update(T value) {
        auto* expected = &storage_[active_index_];
        storage_[active_index_ ^ 1] = std::move(value);

        for (size_t i = 0; i < k_loop_upper_bound; ++i) {
            if (ptr_.compare_exchange_strong(expected, &storage_[active_index_ ^ 1], std::memory_order_acq_rel)) {
                auto prev = std::move(storage_[active_index_]);
                active_index_ ^= 1;
                return prev;
            }
            // Update expected because compare_exchange_strong() failed at this point which means it loaded the actual value into expected.
            expected = &storage_[active_index_];

            if (i >= k_sleep_threshold) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else if (i >= k_yield_threshold) {
                std::this_thread::yield();
            }
        }

        RAV_ASSERT_FALSE("Loop upper bound reached");
        return std::nullopt;
    }

    /**
     * Resets the object to contain a new, default constructed object.
     * @return True if resetting succeeded, or false if not.
     */
    [[nodiscard]] bool reset() {
        return update(T {}).has_value();
    }

  private:
    T storage_[2] {};
    uint8_t active_index_ {0};
    std::atomic<T*> ptr_ {&storage_[0]};
    static_assert(std::atomic<T*>::is_always_lock_free, "Atomic pointer is not lock free");
};

}  // namespace rav
