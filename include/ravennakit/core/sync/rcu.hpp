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

#include <mutex>
#include <atomic>
#include <memory>

namespace rav {

/**
 * This class behaves like a Read-Copy-Update (RCU) synchronization mechanism and allows to share objects among multiple
 * readers which can read the most recent value in a wait-free manner (which also implies lock-free).
 *
 * The writer side is protected by a mutex and can update the value in a thread safe way.
 * It's important to reclaim memory by calling reclaim() periodically to delete outdated values. As long as there are
 * readers using an object, the object and newer objects won't be deleted.
 *
 * To give a realtime thread access to objects, create a reader object and use the read_lock() method to get a lock. The
 * lock provides access to the object through the * and -> operators, as well as the get() method.
 *
 * @tparam T The type of the object to share.
 */
template<class T>
class rcu {
  public:
    /**
     * A reader object gives a single thread access to the most recent value. Store a reader object per thread, and use
     * read_lock to acquire a lock which in turn provides access to the value.
     */
    class reader {
      public:
        /**
         * A lock object provides access to the value. Getting and using a lock is wait-free.
         */
        class realtime_lock {
          public:
            /**
             * Constructs a lock from given reader.
             * All methods are real-time safe (wait-free), but not thread safe.
             * Real-time safe: yes, wait-free.
             * Thread safe: no.
             * @param parent_reader The reader to associate this lock with.
             */
            explicit realtime_lock(reader& parent_reader) : reader_(&parent_reader) {
                if (reader_->num_locks_.fetch_add(1) >= 1) {
                    // In this case we're potentially loading a newer value than our the readers epoch, but this has no
                    // negative side effects.
                    value_ = reader_->owner_.most_recent_value_.load();
                } else {
                    // As long as we progress the epoch forward, there is no aba problem here. The only effect is that a
                    // value might be reclaimed later.
                    reader_->epoch_.store(reader_->owner_.current_epoch_.load());
                    // The value we load might belong to a newer epoch than the one we loaded, but this is no problem
                    // because newer values than the oldest used value are never deleted.
                    value_ = reader_->owner_.most_recent_value_.load();
                }
            }

            ~realtime_lock() {
                reset();
            }

            realtime_lock(const realtime_lock&) = delete;
            realtime_lock& operator=(const realtime_lock&) = delete;
            realtime_lock(realtime_lock&&) = delete;
            realtime_lock& operator=(realtime_lock&&) = delete;

            /**
             * Real-time safe: yes, wait-free.
             * Thread safe: no.
             * @return A reference to the contained object. Reference is only valid if the value is not nullptr.
             */
            const T& operator*() const {
                RAV_ASSERT(value_ != nullptr, "Value is nullptr");
                return *value_;
            }

            /**
             * Real-time safe: yes, wait-free.
             * Thread safe: no.
             * @return A pointer to the contained object, or nullptr if the value is nullptr.
             */
            const T* operator->() const {
                RAV_ASSERT(value_ != nullptr, "Value is nullptr");
                return value_;
            }

            /**
             * Real-time safe: yes, wait-free.
             * Thread safe: no.
             * @return A pointer to the contained object, or nullptr if the value is nullptr.
             */
            T* get() {
                return value_;
            }

            /**
             * Real-time safe: yes, wait-free.
             * Thread safe: no.
             * @return A pointer to the contained object, or nullptr if the value is nullptr.
             */
            const T* get() const {
                return value_;
            }

            /**
             * Resets this lock, releasing the value.
             * Real-time safe: yes, wait-free.
             * Thread safe: no.
             */
            void reset() {
                value_ = nullptr;
                if (reader_ == nullptr) {
                    return;
                }
                reader_->num_locks_.fetch_sub(1);
                RAV_ASSERT_NO_THROW(reader_->num_locks_ >= 0, "Number of locks should be non-negative");
                reader_ = nullptr;
            }

          private:
            reader* reader_ {nullptr};
            T* value_ {nullptr};
        };

        /**
         * Constructs a reader object and registers it with the owner.
         * Real-time safe: no.
         * Thread safe: yes.
         * @param owner The owner of this reader.
         */
        explicit reader(rcu& owner) : owner_(owner) {
            std::lock_guard lock(owner_.readers_mutex_);
            owner_.readers_.push_back(this);
        }

        /**
         * Destructor
         * Real-time safe: no.
         * Thread safe: yes.
         */
        ~reader() {
            std::lock_guard lock(owner_.readers_mutex_);
            owner_.readers_.erase(
                std::remove(owner_.readers_.begin(), owner_.readers_.end(), this), owner_.readers_.end()
            );
        }

        /**
         * Creates a lock object which provides access to the value.
         * Real-time safe: yes, wait-free.
         * Thread safe: no.
         * @return The lock object.
         */
        realtime_lock lock_realtime() {
            return realtime_lock(*this);
        }

      private:
        friend class rcu;
        rcu& owner_;
        std::atomic<uint64_t> epoch_ {0};
        std::atomic<int64_t> num_locks_ {0};
    };

    rcu() = default;

    /**
     * Constructs an rcu object with a new value.
     * Real-time safe: no.
     * Thread safe: yes.
     * @param new_value
     */
    explicit rcu(std::unique_ptr<T> new_value) {
        update(std::move(new_value));
    }

    /**
     * Constructs an rcu object with a new value.
     * Real-time safe: no.
     * Thread safe: yes.
     * @param value
     */
    explicit rcu(T value) {
        update(std::make_unique<T>(std::move(value)));
    }

    /**
     * Real-time safe: no.
     * Thread safe: yes.
     * @return A reader object which uses this rcu object.
     */
    reader create_reader() {
        return reader(*this);
    }

    /**
     * Updates the current value with a new value constructed from the given arguments.
     * Real-time safe: no.
     * Thread safe: yes.
     * @tparam Args The types of the arguments.
     * @param args The arguments to construct the new value.
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
        std::lock_guard lock(values_mutex_);
        most_recent_value_.store(new_value.get());
        // At this point a reader takes most_recent_value_ with current epoch, which is not a problem because newer
        // values than the oldest used value are never deleted.
        auto epoch = current_epoch_.fetch_add(1) + 1;
        values_.emplace_back(epoch_and_value {epoch, std::move(new_value)});
    }

    /**
     * Clears the current value.
     * Real-time safe: no.
     * Thread safe: yes.
     */
    void clear() {
        update(std::unique_ptr<T>());
    }

    /**
     * Reclaims all values which are not used by any reader anymore. Only older objects than the first object used by
     * any reader are deleted.
     * Real-time safe: no.
     * Thread safe: yes.
     * @return The number of values which were reclaimed (deleted).
     */
    [[nodiscard]] size_t reclaim() {
        std::lock_guard lock(values_mutex_);

        RAV_ASSERT(!values_.empty(), "The last value should have never been reclaimed");

        size_t num_reclaimed = 0;
        for (auto it = values_.begin(); it != values_.end() - 1;) {
            std::lock_guard readers_lock(readers_mutex_);
            for (const auto* r : readers_) {
                // r->num_locks_ might be changed by another thread at some point, but this has no consequence because
                // in that case it will load a newer value.
                if (r->num_locks_.load() > 0 && r->epoch_.load() <= it->epoch) {
                    // There is a reader with the epoch of this value, so we can't delete this just yet (or any newer
                    // values).
                    return num_reclaimed;
                }
            }
            it = values_.erase(it);
            ++num_reclaimed;
        }
        return num_reclaimed;
    }

  private:
    struct epoch_and_value {
        uint64_t epoch;
        std::unique_ptr<T> value;
    };

    // Protects the values_ vector.
    std::mutex values_mutex_;

    // Holds the current and previous values.
    std::vector<epoch_and_value> values_;

    // Protects the readers_ vector.
    std::mutex readers_mutex_;

    // Holds the readers.
    std::vector<reader*> readers_;

    // Stores the most recent value.
    std::atomic<T*> most_recent_value_ {nullptr};

    // Holds the current epoch.
    std::atomic<uint64_t> current_epoch_ {};
};

}  // namespace rav
