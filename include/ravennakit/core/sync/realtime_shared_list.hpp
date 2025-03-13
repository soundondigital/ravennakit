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
#include "rcu.hpp"
#include "ravennakit/core/constants.hpp"

#include <vector>
#include <thread>

namespace rav {

/**
 * This class allows to share a list of objects among a single reader in a realtime safe way (wait-free). The writer
 * side is protected by a mutex. Internally a CAS loop is used to update the value in a thread-safe manner.
 * @tparam T The type of the object to share.
 */
template<class T>
class realtime_shared_list {
  public:
    /**
     * A lock object provides access to the value. The lock object is used to access the value in a real-time safe way.
     */
    class realtime_lock {
      public:
        explicit realtime_lock(realtime_shared_list& parent) : parent_(&parent) {
            value_ = parent_->atomic_ptr_.exchange(nullptr);
        }

        ~realtime_lock() {
            reset();
        }

        realtime_lock(const realtime_lock&) = delete;
        realtime_lock& operator=(const realtime_lock&) = delete;
        realtime_lock(realtime_lock&&) = delete;
        realtime_lock& operator=(realtime_lock&&) = delete;

        /**
         * Gets the value at the specified index. Make sure the lock contains a value and the index is within bounds.
         * @param index The index of the value to get.
         * @return A reference to the value at the specified index.
         */
        T& operator[](size_t index) {
            RAV_ASSERT(value_ != nullptr, "Value is nullptr");
            RAV_ASSERT(index < value_->size(), "Index out of bounds");
            return *(*value_)[index];
        }

        /**
         * Gets the value at the specified index. Make sure the lock contains a value and the index is within bounds.
         * @param index The index of the value to get.
         * @return A reference to the value at the specified index.
         */
        const T& operator[](size_t index) const {
            RAV_ASSERT(value_ != nullptr, "Value is nullptr");
            RAV_ASSERT(index < value_->size(), "Index out of bounds");
            return (*value_)[index];
        }

        /**
         * @returns True if the lock contains a value, false otherwise.
         */
        explicit operator bool() const {
            return value_ != nullptr;
        }

        /**
         * Gets the value at the specified index. If the lock doesn't contain a value of if the index is out of bounds,
         * nullptr is returned.
         * @param index The index of the value to get.
         * @return A pointer to the value at the specified index, or nullptr if the value is nullptr or the index is out
         * of bounds.
         */
        T* at(size_t index) {
            if (value_ == nullptr) {
                return nullptr;
            }
            if (index >= value_->size()) {
                return nullptr;
            }
            return &operator[](index);
        }

        /**
         * Gets the value at the specified index. If the lock doesn't contain a value of if the index is out of bounds,
         * nullptr is returned.
         * @param index The index of the value to get.
         * @return A pointer to the value at the specified index, or nullptr if the value is nullptr or the index is out
         * of bounds.
         */
        const T* at(size_t index) const {
            if (value_ == nullptr) {
                return nullptr;
            }
            if (index >= value_->size()) {
                return nullptr;
            }
            return &operator[](index);
        }

        /**
         * Gets the size of the list. If the lock doesn't contain a value, 0 is returned.
         * @return The size of the list.
         */
        [[nodiscard]] size_t size() const {
            return value_ == nullptr ? 0 : value_->size();
        }

        /**
         * Gets the size of the list. If the lock doesn't contain a value, 0 is returned.
         * @return The size of the list.
         */
        [[nodiscard]] size_t empty() const {
            return size() == 0;
        }

        /**
         * Resets the lock to the initial state, releasing the value.
         */
        void reset() {
            parent_->atomic_ptr_.store(value_);
            value_ = nullptr;
        }

        auto begin() {
            return value_ ? value_->begin() : empty_vector_.begin();
        }

        auto end() {
            return value_ ? value_->end() : empty_vector_.end();
        }

        auto begin() const {
            return value_ ? value_->cbegin() : empty_vector_.cbegin();
        }

        auto end() const {
            return value_ ? value_->cend() : empty_vector_.cend();
        }

      private:
        realtime_shared_list* parent_;
        std::vector<T*>* value_ {nullptr};

        static inline const std::vector<T*> empty_vector_ {};
    };

    /**
     * Default constructor.
     */
    realtime_shared_list() {
        std::ignore = rebuild_and_exchange();
    }

    /**
     * @param initial_value Initial value to set. Must not be nullptr.
     */
    explicit realtime_shared_list(std::unique_ptr<T> initial_value) : storage_(std::move(initial_value)) {
        std::ignore = rebuild_and_exchange();
    }

    /**
     * @param initial_value Initial value to set.
     */
    explicit realtime_shared_list(const T& initial_value) : storage_(std::make_unique<T>(initial_value)) {
        std::ignore = rebuild_and_exchange();
    }

    /**
     * @param initial_value Initial value to set.
     */
    explicit realtime_shared_list(T&& initial_value) : storage_(std::make_unique<T>(std::move(initial_value))) {
        std::ignore = rebuild_and_exchange();
    }

    /**
     * Gives access to the list in a wait-free manner, suitable for a realtime thread.
     * @return A lock object that provides access to the list.
     */
    realtime_lock lock_realtime() {
        return realtime_lock(*this);
    }

    /**
     * Pushes a new element to the back of the list. Once the function returns the realtime side will see the new list.
     * Real-time safe: no.
     * Thread safe: yes.
     * @param element The element to push.
     * @return True if the element was successfully pushed, false if the element could not be pushed.
     */
    bool push_back(std::unique_ptr<T> element) {
        std::lock_guard lock(mutex_);
        storage_.push_back(std::move(element));
        return rebuild_and_exchange();
    }

    /**
     * Updates the current value with a new value constructed from the given arguments.
     * Real-time safe: no.
     * Thread safe: yes.
     * @tparam Args
     * @param args
     */
    template<class... Args>
    [[nodiscard]] bool push_back(Args&&... args) {
        return push_back(std::make_unique<T>(std::forward<Args>(args)...));
    }

    /**
     * Erases the element at the specified index. The function might fail if the realtime side is constantly reading the
     * value and there is no time to rebuild the list. In this case the method returns false and the list is not
     * modified.
     * @param index The index of the element to erase.
     * @return True if the element was successfully erased, false if the element could not be erased.
     */
    [[nodiscard]] bool erase(size_t index) {
        std::lock_guard lock(mutex_);
        if (index >= storage_.size()) {
            return false;
        }
        std::unique_ptr<T> element = std::move(storage_[index]);
        storage_.erase(storage_.begin() + static_cast<typename std::vector<T*>::difference_type>(index));
        if (!rebuild_and_exchange()) {
            // Rollback
            storage_.insert(
                storage_.begin() + static_cast<typename std::vector<T*>::difference_type>(index), std::move(element)
            );
            return false;
        }
        return true;
    }

    /**
     * @return True if the list was successfully cleared, false if the list could not be cleared.
     */
    bool clear() {
        std::lock_guard lock(mutex_);
        auto tmp = std::move(storage_);
        storage_.clear();
        if (rebuild_and_exchange()) {
            return true;
        }
        storage_ = std::move(tmp);  // Rollback
        return false;
    }

  private:
    std::vector<std::unique_ptr<T>> storage_;
    std::vector<T*> ptr_storage_a_;
    std::vector<T*> ptr_storage_b_;
    std::vector<T*>* active_ptr_ {&ptr_storage_a_};
    std::atomic<std::vector<T*>*> atomic_ptr_ {&ptr_storage_a_};
    std::mutex mutex_;

    static_assert(std::atomic<std::vector<T*>*>::is_always_lock_free, "Atomic pointer is not lock free");

    [[nodiscard]] bool rebuild_and_exchange() {
        auto* desired = active_ptr_ == &ptr_storage_a_ ? &ptr_storage_b_ : &ptr_storage_a_;

        desired->clear();
        for (const auto& e : storage_) {
            desired->push_back(e.get());
        }

        auto* expected = active_ptr_;

        for (size_t i = 0; i < RAV_LOOP_UPPER_BOUND; ++i) {
            if (atomic_ptr_.compare_exchange_strong(expected, desired)) {
                active_ptr_ = desired;
                return true;
            }
            expected = active_ptr_;
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }

        return false;
    }
};

}  // namespace rav
