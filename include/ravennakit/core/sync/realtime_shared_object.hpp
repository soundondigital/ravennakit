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
 * mutex. Internally a CAS loop is used to update the value in a thread-safe manner.
 * @tparam T The type of the object to share.
 */
template<class T>
class realtime_shared_object {
  public:
    /**
     * A lock object provides access to the value. The lock object is used to access the value in a real-time safe way.
     */
    class realtime_lock {
      public:
        explicit realtime_lock(realtime_shared_object& parent) : parent_(&parent) {
            value_ = parent_->ptr_.exchange(nullptr);
        }

        ~realtime_lock() {
            reset();
        }

        realtime_lock(const realtime_lock&) = delete;
        realtime_lock& operator=(const realtime_lock&) = delete;
        realtime_lock(realtime_lock&&) = delete;
        realtime_lock& operator=(realtime_lock&&) = delete;

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
        const T& operator*() const {
            RAV_ASSERT(value_ != nullptr, "Value is nullptr");
            return *value_;
        }

        /**
         * @return A pointer to the contained object, or nullptr if the value is nullptr.
         */
        const T* operator->() const {
            RAV_ASSERT(value_ != nullptr, "Value is nullptr");
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
        realtime_shared_object* parent_;
        T* value_ {nullptr};
    };

    /**
     * Default constructor.
     */
    realtime_shared_object() : storage_(std::make_unique<T>()), ptr_(storage_.get()) {}

    /**
     * @param initial_value Initial value to set. Must not be nullptr.
     */
    explicit realtime_shared_object(std::unique_ptr<T> initial_value) :
        storage_(std::move(initial_value)), ptr_(storage_.get()) {}

    /**
     * @param initial_value Initial value to set.
     */
    explicit realtime_shared_object(const T& initial_value) :
        storage_(std::make_unique<T>(initial_value)), ptr_(storage_.get()) {}

    /**
     * @param initial_value Initial value to set.
     */
    explicit realtime_shared_object(T&& initial_value) :
        storage_(std::make_unique<T>(std::move(initial_value))), ptr_(storage_.get()) {}

    ~realtime_shared_object() {
        RAV_ASSERT_NO_THROW(ptr_ != nullptr, "There should be no active locks");
    }

    realtime_shared_object(const realtime_shared_object&) = delete;
    realtime_shared_object& operator=(const realtime_shared_object&) = delete;
    realtime_shared_object(realtime_shared_object&&) = delete;
    realtime_shared_object& operator=(realtime_shared_object&&) = delete;

    /**
     * @return Get a lock for realtime access to the current value. During the lifetime of this lock (or until reset is
     * called) no updates can be made to the value.
     * Real-time safe: yes, wait-free
     * Thread safe: no.
     */
    realtime_lock lock_realtime() {
        return realtime_lock(*this);
    }

    /**
     * Updates the current value with a new value constructed from the given arguments.
     * Real-time safe: no.
     * Thread safe: yes.
     * @tparam Args
     * @param args
     */
    template<class... Args>
    void update(Args&&... args) {
        update(std::make_unique<T>(std::forward<Args>(args)...));
    }

    /**
     * Updates the current value with a new value.
     * Real-time safe: no.
     * Thread safe: yes.
     * @param new_value New value to set.
     */
    void update(std::unique_ptr<T> new_value) {
        if (new_value == nullptr) {
            return;
        }

        std::lock_guard lock(mutex_);

        for (auto* expected = storage_.get(); !ptr_.compare_exchange_strong(expected, new_value.get());
             expected = storage_.get()) {
            std::this_thread::yield();
        }

        storage_ = std::move(new_value);
    }

  private:
    std::unique_ptr<T> storage_;
    std::atomic<T*> ptr_ {nullptr};
    static_assert(std::atomic<T*>::is_always_lock_free, "Atomic pointer is not lock free");

    std::mutex mutex_;
};

}  // namespace rav
