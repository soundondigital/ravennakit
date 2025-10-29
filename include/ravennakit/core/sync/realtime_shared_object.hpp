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
#include "ravennakit/core/constants.hpp"

#include <memory>
#include <mutex>
#include <thread>

namespace rav {

/**
 * This class provides a real-time safe way to share objects among a single reader. The writer side is protected by a
 * mutex. Internally a CAS loop is used to update the value in a thread-safe manner.
 * @tparam T The type of the object to share.
 * @tparam loop_upper_bound The maximum number of iterations to perform in the CAS loop. If the loop doesn't succeed in
 * the specified number of iterations, the operation is considered failed.
 */
template<class T, size_t loop_upper_bound = 1'000'000>
class RealtimeSharedObject {
  public:
    /**
     * A lock object provides access to the value. The lock object is used to access the value in a real-time safe way.
     */
    class RealtimeLock {
      public:
        explicit RealtimeLock(RealtimeSharedObject& parent) : parent_(&parent) {
            value_ = parent_->ptr_.exchange(nullptr);
        }

        ~RealtimeLock() {
            reset();
        }

        RealtimeLock(const RealtimeLock&) = delete;
        RealtimeLock& operator=(const RealtimeLock&) = delete;
        RealtimeLock(RealtimeLock&&) = delete;
        RealtimeLock& operator=(RealtimeLock&&) = delete;

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
            parent_->ptr_.store(value_);
            value_ = nullptr;
        }

      private:
        RealtimeSharedObject* parent_;
        T* value_ {nullptr};
    };

    /**
     * Default constructor.
     */
    RealtimeSharedObject() : storage_(std::make_unique<T>()), ptr_(storage_.get()) {}

    /**
     * @param initial_value Initial value to set. Must not be nullptr.
     */
    explicit RealtimeSharedObject(std::unique_ptr<T> initial_value) : storage_(std::move(initial_value)), ptr_(storage_.get()) {}

    /**
     * @param initial_value Initial value to set.
     */
    explicit RealtimeSharedObject(const T& initial_value) : storage_(std::make_unique<T>(initial_value)), ptr_(storage_.get()) {}

    /**
     * @param initial_value Initial value to set.
     */
    explicit RealtimeSharedObject(T&& initial_value) : storage_(std::make_unique<T>(std::move(initial_value))), ptr_(storage_.get()) {}

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
    [[nodiscard]] RealtimeLock lock_realtime() {
        return RealtimeLock(*this);
    }

    /**
     * Updates the current value with a new value constructed from the given arguments.
     * Real-time safe: no.
     * Thread safe: yes.
     * @tparam Args
     * @param args
     * @return True if the value was successfully updated, false if the value could not be updated.
     */
    template<class... Args>
    [[nodiscard]] bool update(Args&&... args) {
        return update(std::make_unique<T>(std::forward<Args>(args)...));
    }

    /**
     * Updates the current value with a new value.
     * Real-time safe: no.
     * Thread safe: yes.
     * @param new_value New value to set.
     * @return True if the value was successfully updated, false if the value could not be updated (when
     * loop_upper_bound was exceeded).
     */
    [[nodiscard]] bool update(std::unique_ptr<T> new_value) {
        if (new_value == nullptr) {
            RAV_ASSERT_FALSE("New value must not be nullptr");
            return false;
        }

        std::lock_guard lock(mutex_);

        auto* expected = storage_.get();

        for (size_t i = 0; i < loop_upper_bound; ++i) {
            if (ptr_.compare_exchange_strong(expected, new_value.get())) {
                storage_ = std::move(new_value);
                return true;
            }
            expected = storage_.get();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        return false;
    }

    /**
     * Resets the object to contain a new, default constructed object.
     * @return True if resetting succeeded, or false if not.
     */
    [[nodiscard]] bool reset() {
        return update(std::make_unique<T>());
    }

  private:
    std::unique_ptr<T> storage_;
    std::atomic<T*> ptr_ {nullptr};
    static_assert(std::atomic<T*>::is_always_lock_free, "Atomic pointer is not lock free");

    std::mutex mutex_;
};

}  // namespace rav
